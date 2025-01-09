/*++
Module Name:
    EngineInit.c

Abstract:
    Implementation of XDMA engine initialization
--*/

#include <ntddk.h>
#include <wdf.h>
#include "EngineInit.h"
#include "Device.h"
#include "DmaStructures.h"

NTSTATUS
XdmaEngineCreate(
    _Out_ PXDMA_ENGINE* Engine,
    _In_ PXDMA_DEVICE_CONTEXT DeviceContext,
    _In_ ULONG Offset,
    _In_ BOOLEAN IsH2C,
    _In_ ULONG Channel
    )
{
    NTSTATUS status;
    u32 val;

    // Set magic number (from Linux driver)
    Engine->Magic = MAGIC_ENGINE;
    Engine->Channel = Channel;

    // Set interrupt bitmask (similar to Linux driver)
    Engine->IrqBitmask = (1 << XDMA_ENG_IRQ_NUM) - 1;
    Engine->IrqBitmask <<= (DeviceContext->EnginesNum * XDMA_ENG_IRQ_NUM);
    Engine->BypassOffset = DeviceContext->EnginesNum * BYPASS_MODE_SPACING;

    // Initialize engine status
    Engine->Status = XDMA_ENGINE_INIT_SUCCESS;

    // Parent device context
    Engine->DeviceContext = DeviceContext;

    // Register addresses (similar to Linux driver)
    Engine->Regs = (PUCHAR)DeviceContext->BarBaseVA[DeviceContext->ConfigBarIdx] + Offset;
    Engine->SgdmaRegs = (PUCHAR)DeviceContext->BarBaseVA[DeviceContext->ConfigBarIdx] + 
                        Offset + SGDMA_OFFSET_FROM_CHANNEL;

    // Check if streaming mode
    val = READ_REGISTER_ULONG((PULONG)&Engine->Regs->Identifier);
    if (val & 0x8000U) {
        Engine->Streaming = TRUE;
    }

    // Set DMA direction
    Engine->IsH2C = IsH2C;

    // Set engine name
    RtlStringCbPrintfA(Engine->Name, sizeof(Engine->Name), "%d-%s%d-%s",
                      DeviceContext->Idx,
                      IsH2C ? "H2C" : "C2H",
                      Channel,
                      Engine->Streaming ? "ST" : "MM");

    // Set descriptor count based on mode
    if (Engine->Streaming && !IsH2C) {
        Engine->DescMax = XDMA_ENGINE_CREDIT_XFER_MAX_DESC;
    } else {
        Engine->DescMax = XDMA_ENGINE_XFER_MAX_DESC;
    }

    // Update device context interrupt masks
    if (IsH2C) {
        DeviceContext->MaskIrqH2C |= Engine->IrqBitmask;
    } else {
        DeviceContext->MaskIrqC2H |= Engine->IrqBitmask;
    }
    DeviceContext->EnginesNum++;

    // Allocate resources
    status = XdmaEngineAllocResource(Engine);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Initialize registers
    status = XdmaEngineInitRegs(Engine);
    if (!NT_SUCCESS(status)) {
        XdmaEngineFreeResource(Engine);
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
XdmaEngineAllocResource(
    _In_ PXDMA_ENGINE Engine
    )
{
    WDFCOMMONBUFFER commonBuffer;
    PHYSICAL_ADDRESS physAddr;
    NTSTATUS status;

    // Allocate descriptor ring (similar to Linux dma_alloc_coherent)
    status = WdfCommonBufferCreate(Engine->DeviceContext->DmaEnabler,
                                 Engine->DescMax * sizeof(XDMA_DESC),
                                 WDF_NO_OBJECT_ATTRIBUTES,
                                 &commonBuffer);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Engine->DescriptorRing = WdfCommonBufferGetVirtualAddress(commonBuffer);
    physAddr = WdfCommonBufferGetAlignedLogicalAddress(commonBuffer);
    Engine->DescriptorRingPhysAddr = physAddr.QuadPart;
    Engine->DescriptorBuffer = commonBuffer;

    // For streaming C2H, allocate result buffer
    if (Engine->Streaming && !Engine->IsH2C) {
        status = WdfCommonBufferCreate(Engine->DeviceContext->DmaEnabler,
                                     Engine->DescMax * sizeof(struct xdma_result),
                                     WDF_NO_OBJECT_ATTRIBUTES,
                                     &commonBuffer);
        if (!NT_SUCCESS(status)) {
            XdmaEngineFreeResource(Engine);
            return status;
        }

        Engine->CyclicResult = WdfCommonBufferGetVirtualAddress(commonBuffer);
        physAddr = WdfCommonBufferGetAlignedLogicalAddress(commonBuffer);
        Engine->CyclicResultPhysAddr = physAddr.QuadPart;
        Engine->CyclicResultBuffer = commonBuffer;
    }

    return STATUS_SUCCESS;
}

VOID
XdmaEngineFreeResource(
    _In_ PXDMA_ENGINE Engine
    )
{
    // Free descriptor ring
    if (Engine->DescriptorBuffer) {
        WdfObjectDelete(Engine->DescriptorBuffer);
        Engine->DescriptorBuffer = NULL;
        Engine->DescriptorRing = NULL;
    }

    // Free cyclic result buffer
    if (Engine->CyclicResultBuffer) {
        WdfObjectDelete(Engine->CyclicResultBuffer);
        Engine->CyclicResultBuffer = NULL;
        Engine->CyclicResult = NULL;
    }
}

NTSTATUS
XdmaEngineInitRegs(
    _In_ PXDMA_ENGINE Engine
    )
{
    u32 val;

    // Clear non-increment addressing mode
    WRITE_REGISTER_ULONG((PULONG)&Engine->Regs->Control,
                        XDMA_CTRL_NON_INCR_ADDR);

    // Configure error interrupts
    val = XDMA_CTRL_IE_DESC_ALIGN_MISMATCH |
          XDMA_CTRL_IE_MAGIC_STOPPED |
          XDMA_CTRL_IE_READ_ERROR |
          XDMA_CTRL_IE_DESC_ERROR;

    // Enable completion interrupts
    val |= XDMA_CTRL_IE_DESC_STOPPED |
           XDMA_CTRL_IE_DESC_COMPLETED;

    // Apply engine configurations
    WRITE_REGISTER_ULONG((PULONG)&Engine->Regs->InterruptEnableMask, val);
    Engine->InterruptEnableMaskValue = val;

    // Enable credit mode for AXI-ST C2H if configured
    if (Engine->Streaming && !Engine->IsH2C) {
        PXDMA_DEVICE_CONTEXT deviceContext = Engine->DeviceContext;
        u32 creditVal = (0x1 << Engine->Channel) << 16;
        struct sgdma_common_regs *commonRegs =
            (struct sgdma_common_regs *)((PUCHAR)deviceContext->BarBaseVA[deviceContext->ConfigBarIdx] +
                                       (0x6 * TARGET_SPACING));

        WRITE_REGISTER_ULONG((PULONG)&commonRegs->CreditModeEnable, creditVal);
    }

    return STATUS_SUCCESS;
}
