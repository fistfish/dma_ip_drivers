/*++
Module Name:
    Driver.h

Abstract:
    Header file for the KMDF driver for Xilinx XDMA
--*/

#include <ntddk.h>
#include <wdf.h>

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD XdmaEvtDriverDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP XdmaEvtDriverContextCleanup;
EVT_WDF_DRIVER_UNLOAD XdmaEvtDriverUnload;

// Driver-specific function declarations
NTSTATUS
XdmaCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );
