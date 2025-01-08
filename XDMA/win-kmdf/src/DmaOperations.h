/*++
Module Name:
    DmaOperations.h

Abstract:
    Header file for DMA operations
--*/

#ifndef __XDMA_DMA_OPERATIONS_H__
#define __XDMA_DMA_OPERATIONS_H__

#include <wdm.h>
#include <ntddk.h>
#include <wdf.h>

// WDM type definitions
#ifndef _PHYSICAL_ADDRESS_DEFINED
#define _PHYSICAL_ADDRESS_DEFINED
typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;
#endif

// WDF DMA types
typedef enum _WDF_DMA_DIRECTION {
    WdfDmaDirectionReadFromDevice = FALSE,
    WdfDmaDirectionWriteToDevice = TRUE
} WDF_DMA_DIRECTION;

// DMA scatter/gather types
typedef struct _SCATTER_GATHER_ELEMENT {
    PHYSICAL_ADDRESS Address;
    ULONG Length;
    ULONG_PTR Reserved;
} SCATTER_GATHER_ELEMENT, *PSCATTER_GATHER_ELEMENT;

typedef struct _SCATTER_GATHER_LIST {
    ULONG NumberOfElements;
    ULONG_PTR Reserved;
    SCATTER_GATHER_ELEMENT Elements[];
} SCATTER_GATHER_LIST, *PSCATTER_GATHER_LIST;

// Forward declarations for WDF types
DECLARE_HANDLE(WDFDMATRANSACTION);
DECLARE_HANDLE(WDFDEVICE);
DECLARE_HANDLE(WDFMEMORY);

// Forward declarations for XDMA types
typedef struct _XDMA_ENGINE XDMA_ENGINE, *PXDMA_ENGINE;
typedef struct _XDMA_TRANSFER XDMA_TRANSFER, *PXDMA_TRANSFER;
typedef struct _XDMA_DESC XDMA_DESC, *PXDMA_DESC;
typedef struct _XDMA_DEVICE_CONTEXT XDMA_DEVICE_CONTEXT, *PXDMA_DEVICE_CONTEXT;

// Engine states
typedef enum _XFER_STATE {
    XFER_STATE_NEW = 0,
    XFER_STATE_SUBMITTED,
    XFER_STATE_COMPLETED,
    XFER_STATE_FAILED,
    XFER_STATE_ABORTED
} XFER_STATE;

// WDF type definitions
typedef VOID (*PFN_XDMA_CYCLIC_CALLBACK)(
    PVOID Context,
    ULONG64 TotalBytes,
    NTSTATUS Status
    );

#include "DmaStructures.h"

EXTERN_C_START

// WDF DMA callback declarations
NTSTATUS
EvtXdmaTransactionConfigure(
    WDFDMATRANSACTION Transaction,
    WDFDEVICE Device,
    PVOID Context,
    WDF_DMA_DIRECTION Direction,
    PSCATTER_GATHER_LIST SgList
    );

BOOLEAN
EvtXdmaTransactionExecute(
    WDFDMATRANSACTION Transaction,
    PVOID Context
    );

VOID
EvtXdmaTransactionDmaTransferred(
    WDFDMATRANSACTION Transaction,
    PVOID Context,
    NTSTATUS Status
    );

VOID
EvtXdmaTransactionDmaCompleted(
    WDFDMATRANSACTION Transaction,
    PVOID Context,
    NTSTATUS Status
    );

// Transaction functions
NTSTATUS
XdmaTransactionConfigure(
    WDFDMATRANSACTION Transaction,
    WDFDEVICE Device,
    PVOID Context,
    WDF_DMA_DIRECTION Direction,
    PSCATTER_GATHER_LIST SgList
    );

BOOLEAN
XdmaTransactionExecute(
    WDFDMATRANSACTION Transaction,
    PVOID Context
    );

VOID
XdmaTransactionDmaTransferred(
    WDFDMATRANSACTION Transaction,
    PVOID Context,
    NTSTATUS Status
    );

VOID
XdmaTransactionDmaCompleted(
    WDFDMATRANSACTION Transaction,
    PVOID Context,
    NTSTATUS Status
    );

// Engine functions
NTSTATUS
XdmaEngineCreate(
    WDFDEVICE Device,
    PXDMA_DEVICE_CONTEXT DeviceContext,
    BOOLEAN IsH2C,
    ULONG Channel,
    PXDMA_ENGINE* Engine
    );

VOID
XdmaEngineDestroy(
    PXDMA_ENGINE Engine
    );

NTSTATUS
XdmaEngineInit(
    PXDMA_ENGINE Engine
    );

VOID
XdmaEngineFreeResource(
    PXDMA_ENGINE Engine
    );

// Transfer functions
NTSTATUS
XdmaTransferCreate(
    PXDMA_ENGINE Engine,
    WDFMEMORY Memory,
    size_t Length,
    BOOLEAN WriteToDevice,
    BOOLEAN IsCyclic,
    PFN_XDMA_CYCLIC_CALLBACK CyclicCallback,
    PVOID CyclicContext,
    PXDMA_TRANSFER* Transfer
    );

VOID
XdmaTransferDestroy(
    PXDMA_TRANSFER Transfer
    );

NTSTATUS
XdmaTransferSubmit(
    PXDMA_ENGINE Engine,
    PXDMA_TRANSFER Transfer
    );

VOID
XdmaTransferComplete(
    PXDMA_ENGINE Engine,
    PXDMA_TRANSFER Transfer,
    NTSTATUS Status
    );

// Descriptor functions
NTSTATUS
XdmaSetupDescriptor(
    PXDMA_DESC Descriptor,
    PHYSICAL_ADDRESS SrcAddr,
    PHYSICAL_ADDRESS DstAddr,
    ULONG Length,
    PHYSICAL_ADDRESS NextAddr,
    BOOLEAN IsLast
    );

EXTERN_C_END

#endif // __XDMA_DMA_OPERATIONS_H__
