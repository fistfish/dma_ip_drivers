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

// All register bits and masks are defined in DmaStructures.h
// This header only contains function declarations and structures specific to engine initialization

// Engine initialization specific constants
#define XDMA_ENGINE_INIT_TIMEOUT     1000    // Milliseconds
#define XDMA_ENGINE_RESET_TIMEOUT    100     // Milliseconds

// Engine initialization status codes
#define XDMA_ENGINE_INIT_SUCCESS     0x0
#define XDMA_ENGINE_INIT_TIMEOUT     0x1
#define XDMA_ENGINE_INIT_ERROR       0x2
#define XDMA_ENGINE_RESET_FAILED     0x3

// Function declarations
NTSTATUS
XdmaEngineCreate(
    _Out_ PXDMA_ENGINE* Engine,
    _In_ PXDMA_DEVICE_CONTEXT DeviceContext,
    _In_ ULONG Offset,
    _In_ BOOLEAN IsH2C,
    _In_ ULONG Channel
    );

NTSTATUS
XdmaEngineInit(
    _In_ PXDMA_ENGINE Engine,
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
