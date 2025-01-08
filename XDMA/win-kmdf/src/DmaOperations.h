/*++
Module Name:
    DmaOperations.h

Abstract:
    Header file for DMA operations
--*/

#ifndef __XDMA_DMA_OPERATIONS_H__
#define __XDMA_DMA_OPERATIONS_H__

#include <ntddk.h>
#include <wdf.h>
#include <sal.h>

// Forward declarations
typedef struct _XDMA_ENGINE XDMA_ENGINE, *PXDMA_ENGINE;
typedef struct _XDMA_TRANSFER XDMA_TRANSFER, *PXDMA_TRANSFER;
typedef struct _XDMA_DESC XDMA_DESC, *PXDMA_DESC;

#include "DmaStructures.h"

EVT_WDF_DMA_TRANSACTION_CONFIGURE XdmaTransactionConfigure;
EVT_WDF_DMA_TRANSACTION_EXECUTE XdmaTransactionExecute;
EVT_WDF_DMA_TRANSACTION_DMA_TRANSFERRED XdmaTransactionDmaTransferred;
EVT_WDF_DMA_TRANSACTION_DMA_COMPLETED XdmaTransactionDmaCompleted;

NTSTATUS
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
    );

BOOLEAN
XdmaTransactionExecute(
    _In_ WDFDMATRANSACTION Transaction,
    _In_ PVOID Context
    );

VOID
XdmaTransactionDmaTransferred(
    _In_ WDFDMATRANSACTION Transaction,
    _In_ PVOID Context,
    _In_ NTSTATUS Status
    );

VOID
XdmaTransactionDmaCompleted(
    _In_ WDFDMATRANSACTION Transaction,
    _In_ PVOID Context,
    _In_ NTSTATUS Status
    );

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
    );

#endif // __XDMA_DMA_OPERATIONS_H__
