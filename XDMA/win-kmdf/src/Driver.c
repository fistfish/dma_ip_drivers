/*++
Module Name:
    Driver.c

Abstract:
    KMDF driver implementation for Xilinx XDMA
    Windows port of the Linux XDMA driver

Environment:
    Kernel-mode Driver Framework
--*/

#include "Device.h"
#include "Driver.h"
#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include "Public.h"
#include "Trace.h"
#include "Driver.tmh"  // Auto-generated WPP trace logging header
#include "version.h"

// Driver version information (from version.h)
#define XDMA_DRIVER_VERSION ((DRV_MOD_MAJOR << 16) | (DRV_MOD_MINOR << 8) | DRV_MOD_PATCHLEVEL)
#define XDMA_DRIVER_VERSION_STRING "XDMA Driver Version " STR(DRV_MOD_MAJOR) "." STR(DRV_MOD_MINOR) "." STR(DRV_MOD_PATCHLEVEL)

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, XdmaEvtDriverDeviceAdd)
#pragma alloc_text (PAGE, XdmaEvtDriverUnload)
#endif

// Xilinx PCIe device IDs (from Linux driver's pci_ids[])
#define XILINX_VENDOR_ID 0x10EE
#define AWS_VENDOR_ID    0x1D0F

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;

    // Initialize WPP Tracing
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    DbgPrint(XDMA_DRIVER_VERSION_STRING "\n");

    // Initialize driver config to control device add
    WDF_DRIVER_CONFIG_INIT(&config, XdmaEvtDriverDeviceAdd);
    
    // Set PnP and power management callbacks
    config.EvtDriverUnload = XdmaEvtDriverUnload;
    config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
    config.DriverPoolTag = 'AMDX';

    // Register a cleanup callback
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = XdmaEvtDriverContextCleanup;

    // Create the driver object
    status = WdfDriverCreate(DriverObject,
                           RegistryPath,
                           &attributes,
                           &config,
                           WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
        DbgPrint("WdfDriverCreate failed with status 0x%x\n", status);
        WPP_CLEANUP(DriverObject);
        return status;
    }

    return status;
}

NTSTATUS
XdmaEvtDriverDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;
    ULONG vendorId, deviceId;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;

    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();

    // Get PCI device information using DEVPKEY
    status = WdfFdoInitQueryProperty(DeviceInit,
                                   &DEVPKEY_Device_VendorID,
                                   DEVPROP_TYPE_UINT32,
                                   sizeof(ULONG),
                                   &vendorId,
                                   NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfFdoInitQueryProperty(DeviceInit,
                                   &DEVPKEY_Device_DeviceID,
                                   DEVPROP_TYPE_UINT32,
                                   sizeof(ULONG),
                                   &deviceId,
                                   NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Check if this is a supported Xilinx or AWS device
    if (vendorId != XILINX_VENDOR_ID && vendorId != AWS_VENDOR_ID) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    // Initialize PnP/Power callbacks
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    // Create the device
    status = XdmaCreateDevice(DeviceInit);
    if (!NT_SUCCESS(status)) {
        DbgPrint("XdmaCreateDevice failed with status 0x%x\n", status);
        return status;
    }

    return status;
}

VOID
XdmaEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
{
    PAGED_CODE();

    // Maps to xpdev_free in Linux driver
    PXDMA_DEVICE_CONTEXT deviceContext = XdmaGetDeviceContext((WDFDEVICE)DriverObject);
    
    // Free device resources
    if (deviceContext != NULL) {
        // Unmap BARs
        for (ULONG i = 0; i < XDMA_BAR_NUM; i++) {
            if (deviceContext->BarBaseVA[i] != NULL) {
                MmUnmapIoSpace(deviceContext->BarBaseVA[i],
                              deviceContext->BarLength[i]);
            }
        }
    }

    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

VOID
XdmaEvtDriverUnload(
    _In_ WDFDRIVER Driver
    )
{
    UNREFERENCED_PARAMETER(Driver);
    PAGED_CODE();
}
