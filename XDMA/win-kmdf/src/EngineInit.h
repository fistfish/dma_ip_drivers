/*++
Module Name:
    EngineInit.h

Abstract:
    Header file for XDMA engine initialization
--*/

#ifndef __XDMA_ENGINE_INIT_H__
#define __XDMA_ENGINE_INIT_H__

#include <ntddk.h>
#include <wdf.h>
#include "Device.h"
#include "DmaStructures.h"

// Engine control register bits (from DmaStructures.h)
// Reuse common definitions from DmaStructures.h to avoid duplicates
#define XDMA_CTRL_IE_INVALID_LEN                 (1UL << 5)
#define XDMA_CTRL_NON_INCR_ADDR                  (1UL << 25)

// Include DmaStructures.h error bits plus our additional ones
#define XDMA_CTRL_IE_ALL_ERRS                    \
        (XDMA_CTRL_IE_DESC_ALIGN_MISMATCH |     \
         XDMA_CTRL_IE_MAGIC_STOPPED |           \
         XDMA_CTRL_IE_INVALID_LEN |             \
         XDMA_CTRL_IE_IDLE_STOPPED |            \
         XDMA_CTRL_IE_READ_ERROR |              \
         XDMA_CTRL_IE_DESC_ERROR)

// Engine status register bits (verified against Linux driver)
#define XDMA_STAT_BUSY                           (1UL << 0)
#define XDMA_STAT_DESC_STOPPED                   (1UL << 1)
#define XDMA_STAT_DESC_COMPLETED                 (1UL << 2)
#define XDMA_STAT_ALIGN_MISMATCH                 (1UL << 3)
#define XDMA_STAT_MAGIC_STOPPED                  (1UL << 4)
#define XDMA_STAT_INVALID_LEN                    (1UL << 5)
#define XDMA_STAT_IDLE_STOPPED                   (1UL << 6)

// Note: Status bits are unique to EngineInit.h and don't conflict with DmaStructures.h

// Function declarations
NTSTATUS
XdmaEngineInit(
    _Out_ PXDMA_ENGINE* Engine,
    _In_ PXDMA_DEVICE_CONTEXT DeviceContext,
    _In_ ULONG Offset,
    _In_ BOOLEAN IsH2C,
    _In_ ULONG Channel
    );

NTSTATUS
XdmaEngineAllocResource(
    _In_ PXDMA_ENGINE Engine
    );

VOID
XdmaEngineFreeResource(
    _In_ PXDMA_ENGINE Engine
    );

NTSTATUS
XdmaEngineInitRegs(
    _In_ PXDMA_ENGINE Engine
    );

#endif // __XDMA_ENGINE_INIT_H__
