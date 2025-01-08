/*++
Module Name:
    Device.c

Abstract:
    Implementation of device-specific functions for XDMA driver
--*/

#include "Device.h"
#include "Public.h"
#include "Trace.h"
#include <ntddk.h>
#include <wdf.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, XdmaCreateDevice)
#pragma alloc_text (PAGE, XdmaEvtDevicePrepareHardware)
#pragma alloc_text (PAGE, XdmaEvtDeviceReleaseHardware)
#pragma alloc_text (PAGE, XdmaEvtDeviceD0Entry)
#pragma alloc_text (PAGE, XdmaEvtDeviceD0Exit)
#endif

NTSTATUS
XdmaCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PXDMA_DEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;
    ULONG vendorId, deviceId;

    // Get PCI device information using DEVPKEY
    DEVPROPTYPE propertyType;
    status = WdfFdoInitQueryPropertyEx(DeviceInit,
                                     &DEVPKEY_Device_VendorID,
                                     sizeof(ULONG),
                                     &vendorId,
                                     &propertyType,
                                     NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = WdfFdoInitQueryPropertyEx(DeviceInit,
                                     &DEVPKEY_Device_DeviceID,
                                     sizeof(ULONG),
                                     &deviceId,
                                     &propertyType,
                                     NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Verify this is a supported Xilinx or AWS device
    if (vendorId != XILINX_VENDOR_ID && vendorId != AWS_VENDOR_ID) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    // For Xilinx vendor ID, verify device ID
    if (vendorId == XILINX_VENDOR_ID) {
        if (deviceId != XILINX_DEVICE_ID_1 && deviceId != XILINX_DEVICE_ID_2) {
            return STATUS_DEVICE_CONFIGURATION_ERROR;
        }
    }

    PAGED_CODE();

    // Set PCI device properties
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_BUS_MASTER);
    WdfDeviceInitSetExclusive(DeviceInit, FALSE);
    
    // Create the device object with our context type
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, XDMA_DEVICE_CONTEXT);
    deviceAttributes.EvtCleanupCallback = XdmaEvtDeviceContextCleanup;

    status = WdfDeviceCreate(&DeviceInit,
                           &deviceAttributes,
                           &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Initialize device context (maps to xpdev_alloc in Linux driver)
    deviceContext = XdmaGetDeviceContext(device);
    deviceContext->WdfDevice = device;
    deviceContext->UserMax = MAX_USER_IRQ;
    deviceContext->H2CChannelMax = XDMA_CHANNEL_NUM_MAX;
    deviceContext->C2HChannelMax = XDMA_CHANNEL_NUM_MAX;

    // Initialize BAR tracking
    RtlZeroMemory(deviceContext->BarBasePA, sizeof(deviceContext->BarBasePA));
    RtlZeroMemory(deviceContext->BarBaseVA, sizeof(deviceContext->BarBaseVA));
    RtlZeroMemory(deviceContext->BarLength, sizeof(deviceContext->BarLength));

    // Configure PnP/power callbacks
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = XdmaEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = XdmaEvtDeviceReleaseHardware;
    pnpPowerCallbacks.EvtDeviceD0Entry = XdmaEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit = XdmaEvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    // Create device interface (maps to xpdev_create_interfaces in Linux driver)
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_XDMA,
        NULL
        );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Create default queue for IOCTLs
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = XdmaEvtIoDeviceControl;

    status = WdfIoQueueCreate(device,
                            &queueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return status;
}

BOOLEAN
XdmaEvtInterruptIsr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ ULONG MessageID
    )
{
    WDFDEVICE device;
    PXDMA_DEVICE_CONTEXT deviceContext;
    PXDMA_INTERRUPT_CONTEXT interruptContext;
    BOOLEAN handled = FALSE;

    device = WdfInterruptGetDevice(Interrupt);
    deviceContext = XdmaGetDeviceContext(device);
    interruptContext = XdmaGetInterruptContext(Interrupt);

    if (interruptContext->IsUserInterrupt) {
        // Handle user interrupt (similar to xdma_user_irq)
        struct interrupt_regs *int_regs = 
            (struct interrupt_regs *)((PUCHAR)deviceContext->BarBaseVA[deviceContext->ConfigBarIdx] + 
                                    XDMA_OFS_INT_CTRL);
        
        // Read and clear user interrupt status
        u32 user_irq_status = READ_REGISTER_ULONG((volatile ULONG *)&int_regs->user_int_request);
        if (user_irq_status) {
            WRITE_REGISTER_ULONG((volatile ULONG *)&int_regs->user_int_request, user_irq_status);
            WdfInterruptQueueDpcForIsr(Interrupt);
            handled = TRUE;
        }
    } else {
        // Handle channel interrupt (similar to xdma_channel_irq)
        struct interrupt_regs *int_regs = 
            (struct interrupt_regs *)((PUCHAR)deviceContext->BarBaseVA[deviceContext->ConfigBarIdx] + 
                                    XDMA_OFS_INT_CTRL);
        
        // Read and clear channel interrupt status
        u32 channel_status = READ_REGISTER_ULONG((volatile ULONG *)&int_regs->channel_int_request);
        if (channel_status & (1 << interruptContext->ChannelId)) {
            WRITE_REGISTER_ULONG((volatile ULONG *)&int_regs->channel_int_request, 
                               1 << interruptContext->ChannelId);
            WdfInterruptQueueDpcForIsr(Interrupt);
            handled = TRUE;
        }
    }

    return handled;
}

VOID
XdmaEvtInterruptDpc(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT AssociatedObject
    )
{
    UNREFERENCED_PARAMETER(AssociatedObject);
    
    WDFDEVICE device = WdfInterruptGetDevice(Interrupt);
    PXDMA_DEVICE_CONTEXT deviceContext = XdmaGetDeviceContext(device);
    PXDMA_INTERRUPT_CONTEXT interruptContext = XdmaGetInterruptContext(Interrupt);

    if (interruptContext->IsUserInterrupt) {
        // Process user interrupt (deferred work)
        // TODO: Implement user interrupt processing
    } else {
        // Process channel interrupt (deferred work)
        // TODO: Implement channel processing
    }
}

NTSTATUS
XdmaEvtInterruptEnable(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFDEVICE AssociatedDevice
    )
{
    UNREFERENCED_PARAMETER(AssociatedDevice);
    
    PXDMA_INTERRUPT_CONTEXT interruptContext = XdmaGetInterruptContext(Interrupt);
    PXDMA_DEVICE_CONTEXT deviceContext = interruptContext->DeviceContext;
    struct interrupt_regs *int_regs;

    int_regs = (struct interrupt_regs *)(deviceContext->BarBaseVA[deviceContext->ConfigBarIdx] + 
                                       XDMA_OFS_INT_CTRL);

    if (interruptContext->IsUserInterrupt) {
        // Enable user interrupt
        WRITE_REGISTER_ULONG((PULONG)&int_regs->user_int_enable_w1s, 
                            1 << interruptContext->ChannelId);
    } else {
        // Enable channel interrupt
        WRITE_REGISTER_ULONG((PULONG)&int_regs->channel_int_enable_w1s,
                            1 << interruptContext->ChannelId);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
XdmaEvtInterruptDisable(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFDEVICE AssociatedDevice
    )
{
    UNREFERENCED_PARAMETER(AssociatedDevice);
    
    PXDMA_INTERRUPT_CONTEXT interruptContext = XdmaGetInterruptContext(Interrupt);
    PXDMA_DEVICE_CONTEXT deviceContext = interruptContext->DeviceContext;
    struct interrupt_regs *int_regs;

    int_regs = (struct interrupt_regs *)(deviceContext->BarBaseVA[deviceContext->ConfigBarIdx] + 
                                       XDMA_OFS_INT_CTRL);

    if (interruptContext->IsUserInterrupt) {
        // Disable user interrupt
        WRITE_REGISTER_ULONG((PULONG)&int_regs->user_int_enable_w1c,
                            1 << interruptContext->ChannelId);
    } else {
        // Disable channel interrupt
        WRITE_REGISTER_ULONG((PULONG)&int_regs->channel_int_enable_w1c,
                            1 << interruptContext->ChannelId);
    }

    return STATUS_SUCCESS;
}

static VOID
XdmaCleanupPartialInterrupts(
    _In_ PXDMA_DEVICE_CONTEXT DeviceContext,
    _In_ ULONG ChannelCount,
    _In_ ULONG UserCount
    )
{
    ULONG i;

    // Clean up channel interrupts
    for (i = 0; i < ChannelCount; i++) {
        if (DeviceContext->ChannelInterrupt[i]) {
            WdfObjectDelete(DeviceContext->ChannelInterrupt[i]);
            DeviceContext->ChannelInterrupt[i] = NULL;
        }
    }

    // Clean up user interrupts
    for (i = 0; i < UserCount; i++) {
        if (DeviceContext->UserInterrupt[i]) {
            WdfObjectDelete(DeviceContext->UserInterrupt[i]);
            DeviceContext->UserInterrupt[i] = NULL;
        }
    }
}

static NTSTATUS
XdmaSetupMsixInterrupts(
    _In_ WDFDEVICE Device,
    _In_ PXDMA_DEVICE_CONTEXT DeviceContext,
    _In_ WDFCMRESLIST ResourceList,
    _In_ WDFCMRESLIST ResourceListTranslated
    )
{
    NTSTATUS status;
    WDF_INTERRUPT_CONFIG interruptConfig;
    WDF_OBJECT_ATTRIBUTES attributes;
    PXDMA_INTERRUPT_CONTEXT interruptContext;
    ULONG i, vector;

    // Set up channel interrupts
    for (i = 0; i < DeviceContext->H2CChannelMax + DeviceContext->C2HChannelMax; i++) {
        WDF_INTERRUPT_CONFIG_INIT(&interruptConfig,
                                XdmaEvtInterruptIsr,
                                XdmaEvtInterruptDpc);
        
        interruptConfig.InterruptTranslated = WdfCmResourceListGetDescriptor(
            ResourceListTranslated, i);
        interruptConfig.InterruptRaw = WdfCmResourceListGetDescriptor(
            ResourceList, i);

        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                              XDMA_INTERRUPT_CONTEXT);

        DbgPrint("Creating channel interrupt %d\n", i);
        status = WdfInterruptCreate(Device,
                                  &interruptConfig,
                                  &attributes,
                                  &DeviceContext->ChannelInterrupt[i]);
        if (!NT_SUCCESS(status)) {
            DbgPrint("Failed to create channel interrupt %d: 0x%x\n", i, status);
            XdmaCleanupPartialInterrupts(DeviceContext, i, 0);
            return status;
        }

        // Initialize interrupt context
        interruptContext = XdmaGetInterruptContext(DeviceContext->ChannelInterrupt[i]);
        interruptContext->DeviceContext = DeviceContext;
        interruptContext->ChannelId = i;
        interruptContext->IsUserInterrupt = FALSE;
        interruptContext->Vector = i;
    }

    // Set up user interrupts
    vector = DeviceContext->H2CChannelMax + DeviceContext->C2HChannelMax;
    for (i = 0; i < DeviceContext->UserMax; i++, vector++) {
        WDF_INTERRUPT_CONFIG_INIT(&interruptConfig,
                                XdmaEvtInterruptIsr,
                                XdmaEvtInterruptDpc);
        
        interruptConfig.InterruptTranslated = WdfCmResourceListGetDescriptor(
            ResourceListTranslated, vector);
        interruptConfig.InterruptRaw = WdfCmResourceListGetDescriptor(
            ResourceList, vector);

        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,
                                              XDMA_INTERRUPT_CONTEXT);

        DbgPrint("Creating user interrupt %d\n", i);
        status = WdfInterruptCreate(Device,
                                  &interruptConfig,
                                  &attributes,
                                  &DeviceContext->UserInterrupt[i]);
        if (!NT_SUCCESS(status)) {
            DbgPrint("Failed to create user interrupt %d: 0x%x\n", i, status);
            XdmaCleanupPartialInterrupts(DeviceContext, 
                                       DeviceContext->H2CChannelMax + DeviceContext->C2HChannelMax,
                                       i);
            return status;
        }

        // Initialize interrupt context
        interruptContext = XdmaGetInterruptContext(DeviceContext->UserInterrupt[i]);
        interruptContext->DeviceContext = DeviceContext;
        interruptContext->ChannelId = i;
        interruptContext->IsUserInterrupt = TRUE;
        interruptContext->Vector = vector;
    }

    // Program MSI-X vectors in hardware (similar to prog_irq_msix_channel/user)
    struct interrupt_regs *int_regs = 
        (struct interrupt_regs *)((PUCHAR)DeviceContext->BarBaseVA[DeviceContext->ConfigBarIdx] + 
                                XDMA_OFS_INT_CTRL);

    // Program channel vectors
    for (i = 0; i < DeviceContext->H2CChannelMax + DeviceContext->C2HChannelMax; i++) {
        WRITE_REGISTER_ULONG((volatile ULONG *)&int_regs->channel_msi_vector[i/4],
                            (i & 0x1f) << ((i % 4) * 8));
    }

    // Program user vectors
    for (i = 0; i < DeviceContext->UserMax; i++) {
        WRITE_REGISTER_ULONG((volatile ULONG *)&int_regs->user_msi_vector[i/4],
                            (i & 0x1f) << ((i % 4) * 8));
    }

    return STATUS_SUCCESS;
}

NTSTATUS
XdmaEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourceList,
    _In_ WDFCMRESLIST ResourceListTranslated
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PXDMA_DEVICE_CONTEXT deviceContext;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
    ULONG i;
    int bar_id_list[XDMA_BAR_NUM] = {0};
    int bar_id_idx = 0;
    int config_bar_pos = 0;

    PAGED_CODE();

    deviceContext = XdmaGetDeviceContext(Device);
    
    // Initialize BAR tracking
    deviceContext->ConfigBarIdx = -1;
    deviceContext->UserBarIdx = -1;
    deviceContext->BypassBarIdx = -1;

    // Get number of resources
    ULONG resourceCount = WdfCmResourceListGetCount(ResourceListTranslated);

    // Iterate through all resources
    for (i = 0; i < resourceCount; i++) {
        descriptor = WdfCmResourceListGetDescriptor(ResourceListTranslated, i);
        
        if (descriptor->Type != CmResourceTypeMemory)
            continue;

        // This is a BAR (memory resource)
        PHYSICAL_ADDRESS physicalAddress = descriptor->u.Memory.Start;
        SIZE_T length = descriptor->u.Memory.Length;
        
        // Map the BAR
        PVOID virtualAddress = MmMapIoSpace(physicalAddress, length, MmNonCached);
        if (!virtualAddress) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        // Store BAR information
        deviceContext->BarBasePA[bar_id_idx] = physicalAddress;
        deviceContext->BarBaseVA[bar_id_idx] = virtualAddress;
        deviceContext->BarLength[bar_id_idx] = length;

        // Try to identify if this is the config BAR
        if ((length >= XDMA_BAR_SIZE) && (deviceContext->ConfigBarIdx < 0)) {
            // Check if this BAR contains XDMA config registers
            struct interrupt_regs *irq_regs = 
                (struct interrupt_regs *)((PUCHAR)virtualAddress + XDMA_OFS_INT_CTRL);
            struct config_regs *cfg_regs = 
                (struct config_regs *)((PUCHAR)virtualAddress + XDMA_OFS_CONFIG);

            ULONG irq_id = READ_REGISTER_ULONG((volatile ULONG *)&irq_regs->identifier);
            ULONG cfg_id = READ_REGISTER_ULONG((volatile ULONG *)&cfg_regs->identifier);

            if (((irq_id & 0xffff0000) == IRQ_BLOCK_ID) &&
                ((cfg_id & 0xffff0000) == CONFIG_BLOCK_ID)) {
                deviceContext->ConfigBarIdx = bar_id_idx;
                config_bar_pos = bar_id_idx;
                DbgPrint("XDMA config BAR found at index %d\n", bar_id_idx);
            }
        }

        bar_id_list[bar_id_idx] = bar_id_idx;
        bar_id_idx++;
    }

    // The XDMA config BAR must be present
    if (deviceContext->ConfigBarIdx < 0) {
        DbgPrint("Failed to detect XDMA config BAR\n");
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        goto cleanup;
    }

    // Set up MSI-X interrupts
    status = XdmaSetupMsixInterrupts(Device, deviceContext, ResourceList, ResourceListTranslated);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to set up MSI-X interrupts: 0x%x\n", status);
        goto cleanup;
    }

    deviceContext->MsixEnabled = TRUE;

    // Initialize DMA engines
    ULONG engineOffset;
    PXDMA_ENGINE engine;

    // Initialize H2C engines
    engineOffset = 0x0000;
    for (i = 0; i < deviceContext->H2CChannelMax; i++) {
        status = XdmaEngineInit(&engine,
                               deviceContext,
                               engineOffset,
                               TRUE,  // H2C
                               i);
        if (!NT_SUCCESS(status)) {
            DbgPrint("Failed to initialize H2C engine %d: 0x%x\n", i, status);
            goto cleanup;
        }
        deviceContext->H2CEngines[i] = engine;
        engineOffset += 0x100;  // Next engine offset
    }

    // Initialize C2H engines
    engineOffset = 0x1000;
    for (i = 0; i < deviceContext->C2HChannelMax; i++) {
        status = XdmaEngineInit(&engine,
                               deviceContext,
                               engineOffset,
                               FALSE,  // C2H
                               i);
        if (!NT_SUCCESS(status)) {
            DbgPrint("Failed to initialize C2H engine %d: 0x%x\n", i, status);
            goto cleanup;
        }
        deviceContext->C2HEngines[i] = engine;
        engineOffset += 0x100;  // Next engine offset
    }

    // Identify user and bypass BARs based on config BAR position
    switch (bar_id_idx) {
    case 1:
        // Only config BAR present
        break;

    case 2:
        if (config_bar_pos == 0) {
            deviceContext->BypassBarIdx = bar_id_list[1];
        } else if (config_bar_pos == 1) {
            deviceContext->UserBarIdx = bar_id_list[0];
        }
        break;

    case 3:
    case 4:
        if ((config_bar_pos == 1) || (config_bar_pos == 2)) {
            deviceContext->UserBarIdx = bar_id_list[0];
            deviceContext->BypassBarIdx = bar_id_list[bar_id_idx - 1];
        }
        break;

    default:
        DbgPrint("Unexpected number of BARs (%d)\n", bar_id_idx);
        break;
    }

    DbgPrint("%d BARs: config %d, user %d, bypass %d\n",
             bar_id_idx,
             deviceContext->ConfigBarIdx,
             deviceContext->UserBarIdx,
             deviceContext->BypassBarIdx);

    return status;

cleanup:
    // Clean up engines
    for (i = 0; i < deviceContext->H2CChannelMax; i++) {
        if (deviceContext->H2CEngines[i]) {
            XdmaEngineFreeResource(deviceContext->H2CEngines[i]);
            ExFreePoolWithTag(deviceContext->H2CEngines[i], 'AMDX');
            deviceContext->H2CEngines[i] = NULL;
        }
    }
    for (i = 0; i < deviceContext->C2HChannelMax; i++) {
        if (deviceContext->C2HEngines[i]) {
            XdmaEngineFreeResource(deviceContext->C2HEngines[i]);
            ExFreePoolWithTag(deviceContext->C2HEngines[i], 'AMDX');
            deviceContext->C2HEngines[i] = NULL;
        }
    }

    // Unmap any mapped BARs
    for (i = 0; i < XDMA_BAR_NUM; i++) {
        if (deviceContext->BarBaseVA[i]) {
            MmUnmapIoSpace(deviceContext->BarBaseVA[i],
                          deviceContext->BarLength[i]);
            deviceContext->BarBaseVA[i] = NULL;
        }
    }
    return status;
}

static VOID
XdmaCleanupInterrupts(
    _In_ PXDMA_DEVICE_CONTEXT DeviceContext
    )
{
    ULONG i;

    // Clean up channel interrupts
    for (i = 0; i < DeviceContext->H2CChannelMax + DeviceContext->C2HChannelMax; i++) {
        if (DeviceContext->ChannelInterrupt[i]) {
            WdfObjectDelete(DeviceContext->ChannelInterrupt[i]);
            DeviceContext->ChannelInterrupt[i] = NULL;
        }
    }

    // Clean up user interrupts
    for (i = 0; i < DeviceContext->UserMax; i++) {
        if (DeviceContext->UserInterrupt[i]) {
            WdfObjectDelete(DeviceContext->UserInterrupt[i]);
            DeviceContext->UserInterrupt[i] = NULL;
        }
    }

    DeviceContext->MsixEnabled = FALSE;
    DeviceContext->MsiEnabled = FALSE;
}

NTSTATUS
XdmaEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    PXDMA_DEVICE_CONTEXT deviceContext;
    ULONG i;

    UNREFERENCED_PARAMETER(ResourcesTranslated);
    PAGED_CODE();

    deviceContext = XdmaGetDeviceContext(Device);

    // Clean up engines first
    for (i = 0; i < deviceContext->H2CChannelMax; i++) {
        if (deviceContext->H2CEngines[i]) {
            XdmaEngineFreeResource(deviceContext->H2CEngines[i]);
            ExFreePoolWithTag(deviceContext->H2CEngines[i], 'AMDX');
            deviceContext->H2CEngines[i] = NULL;
        }
    }
    for (i = 0; i < deviceContext->C2HChannelMax; i++) {
        if (deviceContext->C2HEngines[i]) {
            XdmaEngineFreeResource(deviceContext->C2HEngines[i]);
            ExFreePoolWithTag(deviceContext->C2HEngines[i], 'AMDX');
            deviceContext->C2HEngines[i] = NULL;
        }
    }

    // Clean up interrupts
    XdmaCleanupInterrupts(deviceContext);

    // Unmap all BAR regions
    for (i = 0; i < XDMA_BAR_NUM; i++) {
        if (deviceContext->BarBaseVA[i]) {
            MmUnmapIoSpace(deviceContext->BarBaseVA[i],
                          deviceContext->BarLength[i]);
            deviceContext->BarBaseVA[i] = NULL;
            deviceContext->BarBasePA[i].QuadPart = 0;
            deviceContext->BarLength[i] = 0;
        }
    }

    // Reset BAR indices
    deviceContext->ConfigBarIdx = -1;
    deviceContext->UserBarIdx = -1;
    deviceContext->BypassBarIdx = -1;

    return STATUS_SUCCESS;
}

NTSTATUS
XdmaEvtDeviceD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState
    )
{
    PAGED_CODE();
    return STATUS_SUCCESS;
}

NTSTATUS
XdmaEvtDeviceD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState
    )
{
    PAGED_CODE();
    return STATUS_SUCCESS;
}

// Forward declaration of IOCTL handler from IoControl.c
VOID
XdmaIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    );

VOID
XdmaEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    // Forward the IOCTL request to our comprehensive handler
    XdmaIoDeviceControl(Queue, Request, OutputBufferLength, InputBufferLength, IoControlCode);
}
