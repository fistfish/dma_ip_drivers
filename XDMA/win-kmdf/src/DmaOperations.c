/*++
Module Name:
    DmaOperations.c

Abstract:
    Implementation of DMA operations for XDMA Windows driver
--*/

#include <ntddk.h>
#include <wdf.h>
#include "Device.h"
#include "DmaStructures.h"
#include "DmaOperations.h"

// WDF DMA callback implementations
NTSTATUS
EvtXdmaTransactionConfigure(
    WDFDMATRANSACTION Transaction,
    WDFDEVICE Device,
    PVOID Context,
    WDF_DMA_DIRECTION Direction,
    PSCATTER_GATHER_LIST SgList
    )
{
    return XdmaTransactionConfigure(Transaction, Device, Context, Direction, SgList);
}

BOOLEAN
EvtXdmaTransactionExecute(
    WDFDMATRANSACTION Transaction,
    PVOID Context
    )
{
    return XdmaTransactionExecute(Transaction, Context);
}

VOID
EvtXdmaTransactionDmaTransferred(
    WDFDMATRANSACTION Transaction,
    PVOID Context,
    NTSTATUS Status
    )
{
    XdmaTransactionDmaTransferred(Transaction, Context, Status);
}

VOID
EvtXdmaTransactionDmaCompleted(
    WDFDMATRANSACTION Transaction,
    PVOID Context,
    NTSTATUS Status
    )
{
    XdmaTransactionDmaCompleted(Transaction, Context, Status);
}

// Forward declarations
_Must_inspect_result_
NTSTATUS
NTAPI
XdmaSetupDescriptor(
    _In_ PXDMA_DESC Descriptor,
    _In_ PHYSICAL_ADDRESS SrcAddr,
    _In_ PHYSICAL_ADDRESS DstAddr,
    _In_ ULONG Length,
    _In_ PHYSICAL_ADDRESS NextAddr,
    _In_ BOOLEAN IsLast
    );

NTSTATUS
XdmaTransactionConfigure(
    _In_ WDFDMATRANSACTION Transaction,
    _In_ WDFDEVICE Device,
    _In_ PVOID Context,
    _In_ WDF_DMA_DIRECTION Direction,
    _In_ PSCATTER_GATHER_LIST SgList
    )
{
    PXDMA_TRANSFER transfer = (PXDMA_TRANSFER)Context;
    PXDMA_ENGINE engine = transfer->Engine;
    ULONG i;
    PHYSICAL_ADDRESS nextAddr;
    PHYSICAL_ADDRESS zeroAddr;
    BOOLEAN isH2C = (Direction == WdfDmaDirectionWriteToDevice);

    UNREFERENCED_PARAMETER(Transaction);
    UNREFERENCED_PARAMETER(Device);

    zeroAddr.QuadPart = 0;

    // Validate transfer parameters
    if (transfer->Offset >= engine->DeviceSize) {
        DbgPrint("XdmaTransactionConfigure: Invalid device offset 0x%llx\n",
                 transfer->Offset);
        return STATUS_INVALID_PARAMETER;
    }

    // Set up descriptors for each SG element
    for (i = 0; i < SgList->NumberOfElements; i++) {
        PHYSICAL_ADDRESS srcAddr, dstAddr;
        ULONG64 deviceOffset = transfer->Offset + transfer->BytesTransferred;
        
        // Validate device offset
        if (deviceOffset >= engine->DeviceSize) {
            DbgPrint("XdmaTransactionConfigure: Transfer exceeds device size\n");
            return STATUS_INVALID_PARAMETER;
        }
        
        if (isH2C) {
            srcAddr = SgList->Elements[i].Address;
            dstAddr.QuadPart = engine->DeviceAddress + deviceOffset;
        } else {
            srcAddr.QuadPart = engine->DeviceAddress + deviceOffset;
            dstAddr = SgList->Elements[i].Address;
        }
        
        // Update bytes transferred
        transfer->BytesTransferred += SgList->Elements[i].Length;

        // Calculate next descriptor address
        if (i < SgList->NumberOfElements - 1) {
            nextAddr.QuadPart = transfer->DescriptorPhys.QuadPart + 
                               ((i + 1) * sizeof(XDMA_DESC));
        } else {
            nextAddr = zeroAddr; // Last descriptor
        }

        status = XdmaSetupDescriptor(&transfer->DescriptorVirt[i],
                                    srcAddr,
                                    dstAddr,
                                    SgList->Elements[i].Length,
                                    nextAddr,
                                    (i == SgList->NumberOfElements - 1));
        if (!NT_SUCCESS(status)) {
            DbgPrint("XdmaTransactionConfigure: Failed to setup descriptor %d\n", i);
            return status;
        }
    }

    return STATUS_SUCCESS;
}

BOOLEAN
XdmaTransactionExecute(
    _In_ WDFDMATRANSACTION Transaction,
    _In_ PVOID Context
    )
{
    PXDMA_TRANSFER transfer = (PXDMA_TRANSFER)Context;
    PXDMA_ENGINE engine = transfer->Engine;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(Transaction);

    // Enable interrupts
    WRITE_REGISTER_ULONG((PULONG)&engine->Regs->interrupt_enable_mask,
                        XDMA_CTRL_IE_DESC_STOPPED |
                        XDMA_CTRL_IE_DESC_COMPLETED |
                        XDMA_CTRL_IE_DESC_ALIGN_MISMATCH |
                        XDMA_CTRL_IE_MAGIC_STOPPED |
                        XDMA_CTRL_IE_IDLE_STOPPED |
                        XDMA_CTRL_IE_READ_ERROR |
                        XDMA_CTRL_IE_DESC_ERROR);

    // Start the engine
    WRITE_REGISTER_ULONG((PULONG)&engine->Regs->control,
                        XDMA_CTRL_RUN_STOP |
                        (engine->NonIncrementingAddr ? XDMA_CTRL_NON_INCR_ADDR : 0));

    return TRUE;
}

VOID
XdmaTransactionDmaTransferred(
    _In_ WDFDMATRANSACTION Transaction,
    _In_ PVOID Context,
    _In_ NTSTATUS Status
    )
{
    PXDMA_TRANSFER transfer = (PXDMA_TRANSFER)Context;
    
    UNREFERENCED_PARAMETER(Transaction);

    if (!NT_SUCCESS(Status)) {
        DbgPrint("XdmaTransactionDmaTransferred: Transfer failed with status 0x%x\n",
                 Status);
        transfer->State = XFER_STATE_FAILED;
    }
}

VOID
XdmaTransactionDmaCompleted(
    _In_ WDFDMATRANSACTION Transaction,
    _In_ PVOID Context,
    _In_ NTSTATUS Status
    )
{
    PXDMA_TRANSFER transfer = (PXDMA_TRANSFER)Context;
    PXDMA_ENGINE engine = transfer->Engine;
    
    UNREFERENCED_PARAMETER(Transaction);

    LARGE_INTEGER currentTime, elapsedTime;
    KeQuerySystemTime(&currentTime);
    elapsedTime.QuadPart = currentTime.QuadPart - transfer->StartTime.QuadPart;
    
    // Update performance metrics
    transfer->TotalBytes += transfer->BytesTransferred;
    transfer->TotalTime += (ULONG64)(elapsedTime.QuadPart / 10); // Convert to microseconds

    if (!NT_SUCCESS(Status)) {
        DbgPrint("XdmaTransactionDmaCompleted: Transfer failed with status 0x%x\n",
                 Status);
        transfer->State = XFER_STATE_FAILED;
    } else {
        transfer->State = XFER_STATE_COMPLETED;
        
        // Handle cyclic transfers
        if (transfer->IsCyclic) {
            // Reset transfer state for next iteration
            transfer->BytesTransferred = 0;
            transfer->State = XFER_STATE_NEW;
            KeQuerySystemTime(&transfer->StartTime);
            
            // Notify callback if registered
            if (transfer->CyclicCallback && transfer->CyclicCallbackFn) {
                transfer->CyclicCallbackFn(transfer->CyclicContext,
                                         transfer->TotalBytes,
                                         Status);
            }
            
            // Resubmit the transfer
            Status = XdmaTransferSubmit(engine, transfer);
            if (NT_SUCCESS(Status)) {
                return; // Don't complete the transfer
            }
        }
    }

    // Complete the transfer (non-cyclic or failed cyclic)
    XdmaTransferComplete(engine, transfer, Status);
}

NTSTATUS
XdmaSetupDescriptor(
    _In_ PXDMA_DESC Descriptor,
    _In_ PHYSICAL_ADDRESS SrcAddr,
    _In_ PHYSICAL_ADDRESS DstAddr,
    _In_ ULONG Length,
    _In_ PHYSICAL_ADDRESS NextAddr,
    _In_ BOOLEAN IsLast
    )
{
    // Validate parameters
    if (Length == 0 || Length > XDMA_DESC_BLEN_MAX) {
        DbgPrint("XdmaSetupDescriptor: Invalid length %d\n", Length);
        return STATUS_INVALID_PARAMETER;
    }

    if (SrcAddr.QuadPart == 0 || DstAddr.QuadPart == 0) {
        DbgPrint("XdmaSetupDescriptor: Invalid address src=0x%llx dst=0x%llx\n",
                 SrcAddr.QuadPart, DstAddr.QuadPart);
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Descriptor, sizeof(XDMA_DESC));

    // Set up descriptor fields
    Descriptor->Control = IsLast ? XDMA_DESC_EOP : 0;
    Descriptor->Bytes = Length;
    Descriptor->SrcAddrLow = (ULONG)SrcAddr.LowPart;
    Descriptor->SrcAddrHigh = (ULONG)SrcAddr.HighPart;
    Descriptor->DstAddrLow = (ULONG)DstAddr.LowPart;
    Descriptor->DstAddrHigh = (ULONG)DstAddr.HighPart;
    Descriptor->NextLow = (ULONG)NextAddr.LowPart;
    Descriptor->NextHigh = (ULONG)NextAddr.HighPart;

    DbgPrint("XdmaSetupDescriptor: Created descriptor src=0x%llx dst=0x%llx len=%d%s\n",
             SrcAddr.QuadPart, DstAddr.QuadPart, Length,
             IsLast ? " (EOP)" : "");

    return STATUS_SUCCESS;
}

#define XDMA_DMA_DESCRIPTOR_RING_SIZE 256

NTSTATUS
XdmaEngineCreate(
    _In_ WDFDEVICE Device,
    _In_ PXDMA_DEVICE_CONTEXT DeviceContext,
    _In_ BOOLEAN IsH2C,
    _In_ ULONG Channel,
    _Out_ PXDMA_ENGINE *Engine
    )
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    PXDMA_ENGINE engine;
    WDF_DMA_ENABLER_CONFIG dmaConfig;
    size_t descriptorBufferSize;

    // Allocate and initialize engine object
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, XDMA_ENGINE);
    status = WdfObjectCreate(&attributes, (WDFOBJECT*)&engine);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Initialize engine fields
    engine->DeviceContext = DeviceContext;
    engine->IsH2C = IsH2C;
    engine->Channel = Channel;
    engine->Running = FALSE;

    // Create spin lock for engine
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = WdfSpinLockCreate(&attributes, &engine->Lock);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    // Initialize transfer queue
    InitializeListHead(&engine->TransferQueue);

    // Configure DMA enabler
    WDF_DMA_ENABLER_CONFIG_INIT(&dmaConfig, WdfDmaProfileScatterGather64Duplex, 
                               XDMA_DESC_BLEN_MAX);
    dmaConfig.WdmDmaVersionOverride = 3;
    
    status = WdfDmaEnablerCreate(Device, &dmaConfig, WDF_NO_OBJECT_ATTRIBUTES,
                                &engine->DmaEnabler);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    // Allocate descriptor ring buffer
    descriptorBufferSize = sizeof(XDMA_DESC) * XDMA_DMA_DESCRIPTOR_RING_SIZE;
    status = WdfCommonBufferCreate(engine->DmaEnabler, descriptorBufferSize,
                                 WDF_NO_OBJECT_ATTRIBUTES, &engine->DescriptorBuffer);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    // Get virtual and physical addresses of descriptor buffer
    engine->DescriptorRing = WdfCommonBufferGetAlignedVirtualAddress(engine->DescriptorBuffer);
    engine->DescriptorRingSize = XDMA_DMA_DESCRIPTOR_RING_SIZE;
    
    // Map engine registers
    if (IsH2C) {
        engine->Regs = (struct engine_regs *)(DeviceContext->BarBaseVA[DeviceContext->ConfigBarIdx] +
                                            H2C_CHANNEL_OFFSET + Channel * CHANNEL_SPACING);
        engine->SgdmaRegs = (struct engine_sgdma_regs *)(DeviceContext->BarBaseVA[DeviceContext->ConfigBarIdx] +
                                                       SGDMA_OFFSET_FROM_CHANNEL + 
                                                       H2C_CHANNEL_OFFSET + 
                                                       Channel * CHANNEL_SPACING);
    } else {
        engine->Regs = (struct engine_regs *)(DeviceContext->BarBaseVA[DeviceContext->ConfigBarIdx] +
                                            Channel * CHANNEL_SPACING);
        engine->SgdmaRegs = (struct engine_sgdma_regs *)(DeviceContext->BarBaseVA[DeviceContext->ConfigBarIdx] +
                                                       SGDMA_OFFSET_FROM_CHANNEL + 
                                                       Channel * CHANNEL_SPACING);
    }

    *Engine = engine;
    return STATUS_SUCCESS;

cleanup:
    if (engine->DescriptorBuffer) {
        WdfObjectDelete(engine->DescriptorBuffer);
    }
    if (engine->DmaEnabler) {
        WdfObjectDelete(engine->DmaEnabler);
    }
    if (engine->Lock) {
        WdfObjectDelete(engine->Lock);
    }
    WdfObjectDelete(engine);
    return status;
}

VOID
XdmaEngineDestroy(
    _In_ PXDMA_ENGINE Engine
    )
{
    // Stop the engine
    WRITE_REGISTER_ULONG((PULONG)&Engine->Regs->control, 0);
    
    // Clean up all pending transfers
    while (!IsListEmpty(&Engine->TransferQueue)) {
        PLIST_ENTRY entry = RemoveHeadList(&Engine->TransferQueue);
        PXDMA_TRANSFER transfer = CONTAINING_RECORD(entry, XDMA_TRANSFER, TransferList);
        XdmaTransferDestroy(transfer);
    }

    // Delete engine objects
    if (Engine->DescriptorBuffer) {
        WdfObjectDelete(Engine->DescriptorBuffer);
    }
    if (Engine->DmaEnabler) {
        WdfObjectDelete(Engine->DmaEnabler);
    }
    if (Engine->Lock) {
        WdfObjectDelete(Engine->Lock);
    }
    WdfObjectDelete(Engine);
}

NTSTATUS
XdmaTransferCreate(
    _In_ PXDMA_ENGINE Engine,
    _In_ WDFMEMORY Memory,
    _In_ size_t Length,
    _In_ BOOLEAN WriteToDevice,
    _In_ BOOLEAN IsCyclic,
    _In_opt_ PFN_XDMA_CYCLIC_CALLBACK CyclicCallback,
    _In_opt_ PVOID CyclicContext,
    _Out_ PXDMA_TRANSFER *Transfer
    )
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    PXDMA_TRANSFER transfer;
    WDF_DMA_TRANSACTION_CONFIG dmaConfig;
    size_t descriptorSize;

    // Allocate transfer object
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, XDMA_TRANSFER);
    status = WdfObjectCreate(&attributes, (WDFOBJECT*)&transfer);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Initialize transfer
    transfer->Length = Length;
    transfer->State = XFER_STATE_NEW;
    transfer->IsCyclic = IsCyclic;
    transfer->CyclicCallback = (CyclicCallback != NULL);
    transfer->CyclicCallbackFn = CyclicCallback;
    transfer->CyclicContext = CyclicContext;
    transfer->BytesTransferred = 0;
    transfer->TotalBytes = 0;
    transfer->TotalTime = 0;
    KeQuerySystemTime(&transfer->StartTime);
    InitializeListHead(&transfer->TransferList);

    // Calculate number of descriptors needed
    ULONG descriptorCount = (ULONG)((Length + XDMA_DESC_BLEN_MAX - 1) / XDMA_DESC_BLEN_MAX);
    descriptorSize = sizeof(XDMA_DESC) * descriptorCount;

    // Allocate descriptor buffer
    status = WdfCommonBufferCreate(Engine->DmaEnabler, descriptorSize,
                                 WDF_NO_OBJECT_ATTRIBUTES, &transfer->CommonBuffer);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    // Get virtual and physical addresses
    transfer->CommonBufferVirt = WdfCommonBufferGetAlignedVirtualAddress(transfer->CommonBuffer);
    transfer->CommonBufferPhys = WdfCommonBufferGetAlignedLogicalAddress(transfer->CommonBuffer);
    transfer->DescriptorVirt = (PXDMA_DESC)transfer->CommonBufferVirt;
    transfer->DescriptorPhys = transfer->CommonBufferPhys;
    transfer->DescriptorCount = descriptorCount;

    // Create DMA transaction
    WDF_DMA_TRANSACTION_CONFIG_INIT(&dmaConfig, WdfDmaDirectionWriteToDevice);
    status = WdfDmaTransactionCreate(Engine->DmaEnabler, WDF_NO_OBJECT_ATTRIBUTES,
                                   &transfer->DmaTransaction);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    // Initialize DMA transaction
    status = WdfDmaTransactionInitialize(transfer->DmaTransaction, 
                                       XdmaTransactionExecute,
                                       WriteToDevice ? WdfDmaDirectionWriteToDevice 
                                                   : WdfDmaDirectionReadFromDevice,
                                       Memory, 0, Length);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    *Transfer = transfer;
    return STATUS_SUCCESS;

cleanup:
    if (transfer->DmaTransaction) {
        WdfObjectDelete(transfer->DmaTransaction);
    }
    if (transfer->CommonBuffer) {
        WdfObjectDelete(transfer->CommonBuffer);
    }
    WdfObjectDelete(transfer);
    return status;
}

VOID
XdmaTransferDestroy(
    _In_ PXDMA_TRANSFER Transfer
    )
{
    // Stop cyclic transfers if active
    if (Transfer->IsCyclic && Transfer->State != XFER_STATE_NEW) {
        DbgPrint("XdmaTransferDestroy: Stopping cyclic transfer\n");
        Transfer->State = XFER_STATE_ABORTED;
        
        // Notify callback of cleanup if registered
        if (Transfer->CyclicCallback && Transfer->CyclicCallbackFn) {
            Transfer->CyclicCallbackFn(Transfer->CyclicContext,
                                     Transfer->TotalBytes,
                                     STATUS_CANCELLED);
        }
    }

    // Remove from transfer queue if still queued
    if (!IsListEmpty(&Transfer->TransferList)) {
        RemoveEntryList(&Transfer->TransferList);
    }

    // Clean up DMA resources
    if (Transfer->DmaTransaction) {
        WdfObjectDelete(Transfer->DmaTransaction);
    }
    if (Transfer->CommonBuffer) {
        WdfObjectDelete(Transfer->CommonBuffer);
    }
    WdfObjectDelete(Transfer);
}

NTSTATUS
XdmaTransferSubmit(
    _In_ PXDMA_ENGINE Engine,
    _In_ PXDMA_TRANSFER Transfer
    )
{
    NTSTATUS status;
    WdfSpinLockAcquire(Engine->Lock);

    // Add to transfer queue
    InsertTailList(&Engine->TransferQueue, &Transfer->TransferList);
    Transfer->State = XFER_STATE_SUBMITTED;

    // Start DMA transaction
    status = WdfDmaTransactionExecute(Transfer->DmaTransaction, Transfer);
    if (!NT_SUCCESS(status)) {
        DbgPrint("XdmaTransferSubmit: WdfDmaTransactionExecute failed with status 0x%x\n", status);
        RemoveEntryList(&Transfer->TransferList);
        Transfer->State = XFER_STATE_FAILED;
        WdfSpinLockRelease(Engine->Lock);
        return status;
    }

    // Ensure descriptor writes are visible to hardware
    KeMemoryBarrier();

    // Program the engine with first descriptor
    WRITE_REGISTER_ULONG((PULONG)&Engine->SgdmaRegs->first_desc_lo, 
                        (ULONG)Transfer->DescriptorPhys.LowPart);
    WRITE_REGISTER_ULONG((PULONG)&Engine->SgdmaRegs->first_desc_hi, 
                        (ULONG)Transfer->DescriptorPhys.HighPart);
    WRITE_REGISTER_ULONG((PULONG)&Engine->SgdmaRegs->first_desc_adjacent,
                        Transfer->DescriptorCount - 1);

    // Ensure register writes complete before starting engine
    KeMemoryBarrier();
    READ_REGISTER_ULONG((PULONG)&Engine->SgdmaRegs->first_desc_adjacent);

    DbgPrint("XdmaTransferSubmit: Started transfer with %d descriptors at PA 0x%llx\n",
             Transfer->DescriptorCount,
             Transfer->DescriptorPhys.QuadPart);

    // Start engine if not already running
    if (!Engine->Running) {
        WRITE_REGISTER_ULONG((PULONG)&Engine->Regs->control, XDMA_CTRL_RUN_STOP);
        Engine->Running = TRUE;
    }

    WdfSpinLockRelease(Engine->Lock);
    return STATUS_SUCCESS;
}

VOID
XdmaTransferComplete(
    _In_ PXDMA_ENGINE Engine,
    _In_ PXDMA_TRANSFER Transfer,
    _In_ NTSTATUS Status
    )
{
    WdfSpinLockAcquire(Engine->Lock);

    // Update transfer state
    Transfer->State = NT_SUCCESS(Status) ? XFER_STATE_COMPLETED : XFER_STATE_FAILED;

    // Remove from transfer queue
    RemoveEntryList(&Transfer->TransferList);

    // Stop engine if no more transfers
    if (IsListEmpty(&Engine->TransferQueue)) {
        WRITE_REGISTER_ULONG((PULONG)&Engine->Regs->control, 0);
        Engine->Running = FALSE;
    }

    WdfSpinLockRelease(Engine->Lock);

    // Complete the DMA transaction
    WdfDmaTransactionDmaCompleted(Transfer->DmaTransaction, Status);
    WdfDmaTransactionRelease(Transfer->DmaTransaction);
}
