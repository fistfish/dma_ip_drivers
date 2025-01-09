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
#include <wdfDmaEnabler.h>
#include "DmaStructures.h"

// Forward declarations for XDMA types
typedef struct _XDMA_ENGINE XDMA_ENGINE, *PXDMA_ENGINE;
typedef struct _XDMA_TRANSFER XDMA_TRANSFER, *PXDMA_TRANSFER;
typedef struct _XDMA_DESC XDMA_DESC, *PXDMA_DESC;
typedef struct _XDMA_DEVICE_CONTEXT XDMA_DEVICE_CONTEXT, *PXDMA_DEVICE_CONTEXT;

// Engine states and register definitions from DmaStructures.h
#include <wdfdmaenabler.h>
#include <wdfDmaEnabler.h>

// WDF type definitions
typedef VOID (*PFN_XDMA_CYCLIC_CALLBACK)(
    PVOID Context,
    ULONG64 TotalBytes,
    NTSTATUS Status
    );

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
    __in WDFDEVICE Device,
    __in PXDMA_DEVICE_CONTEXT DeviceContext,
    __in BOOLEAN IsH2C,
    __in ULONG Channel,
    __out PXDMA_ENGINE* Engine
    );

VOID
XdmaEngineDestroy(
    PXDMA_ENGINE Engine
    );

NTSTATUS
XdmaEngineInit(
    __in PXDMA_ENGINE Engine,
    __in ULONG Offset,
    __in BOOLEAN IsH2C,
    __in ULONG Channel
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
