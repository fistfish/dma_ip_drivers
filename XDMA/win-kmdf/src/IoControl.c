/*++
Module Name:
    IoControl.c

Abstract:
    IOCTL handler implementation for XDMA Windows driver
--*/

#include <ntddk.h>
#include <wdf.h>
#include "Device.h"
#include "DmaStructures.h"
#include "DmaOperations.h"
#include "Public.h"

VOID
XdmaIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PXDMA_DEVICE_CONTEXT deviceContext = XdmaGetDeviceContext(device);
    PVOID inputBuffer = NULL;
    PVOID outputBuffer = NULL;
    size_t bytesReturned = 0;

    // Get input and output buffers
    if (InputBufferLength > 0) {
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(XDMA_IOC_BASE),
                                             &inputBuffer, NULL);
        if (!NT_SUCCESS(status)) {
            goto exit;
        }

        // Validate magic number
        PXDMA_IOC_BASE base = (PXDMA_IOC_BASE)inputBuffer;
        if (base->Magic != XDMA_MAGIC) {
            status = STATUS_INVALID_PARAMETER;
            goto exit;
        }
    }

    if (OutputBufferLength > 0) {
        status = WdfRequestRetrieveOutputBuffer(Request, sizeof(XDMA_IOC_BASE),
                                              &outputBuffer, NULL);
        if (!NT_SUCCESS(status)) {
            goto exit;
        }
    }

    switch (IoControlCode) {
    case IOCTL_XDMA_GET_VERSION:
        {
            if (OutputBufferLength < sizeof(XDMA_VERSION_INFO)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PXDMA_VERSION_INFO versionInfo = (PXDMA_VERSION_INFO)outputBuffer;
            versionInfo->Major = DRV_MOD_MAJOR;
            versionInfo->Minor = DRV_MOD_MINOR;
            versionInfo->Build = DRV_MOD_PATCHLEVEL;
            bytesReturned = sizeof(XDMA_VERSION_INFO);
        }
        break;

    case IOCTL_XDMA_IOCINFO:
        {
            if (OutputBufferLength < sizeof(XDMA_IOC_INFO)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PXDMA_IOC_INFO info = (PXDMA_IOC_INFO)outputBuffer;
            info->Vendor = deviceContext->VendorId;
            info->Device = deviceContext->DeviceId;
            info->SubsystemVendor = deviceContext->SubsystemVendorId;
            info->SubsystemDevice = deviceContext->SubsystemId;
            info->DmaEngineVersion = deviceContext->DmaEngineVersion;
            info->DriverVersion = (DRV_MOD_MAJOR << 16) | (DRV_MOD_MINOR << 8) | DRV_MOD_PATCHLEVEL;
            info->FeatureId = deviceContext->FeatureId;
            info->Domain = deviceContext->Domain;
            info->Bus = deviceContext->Bus;
            info->Dev = deviceContext->Dev;
            info->Func = deviceContext->Func;
            bytesReturned = sizeof(XDMA_IOC_INFO);
        }
        break;

    case IOCTL_XDMA_IOCOFFLINE:
        {
            // Stop all DMA engines
            for (ULONG i = 0; i < deviceContext->H2CChannelMax; i++) {
                if (deviceContext->H2CEngines[i]) {
                    WRITE_REGISTER_ULONG((PULONG)&deviceContext->H2CEngines[i]->Regs->control, 0);
                    deviceContext->H2CEngines[i]->Running = FALSE;
                }
            }
            for (ULONG i = 0; i < deviceContext->C2HChannelMax; i++) {
                if (deviceContext->C2HEngines[i]) {
                    WRITE_REGISTER_ULONG((PULONG)&deviceContext->C2HEngines[i]->Regs->control, 0);
                    deviceContext->C2HEngines[i]->Running = FALSE;
                }
            }
        }
        break;

    case IOCTL_XDMA_IOCONLINE:
        {
            // Re-enable all DMA engines
            for (ULONG i = 0; i < deviceContext->H2CChannelMax; i++) {
                if (deviceContext->H2CEngines[i]) {
                    WRITE_REGISTER_ULONG((PULONG)&deviceContext->H2CEngines[i]->Regs->control,
                                       XDMA_CTRL_RUN_STOP);
                    deviceContext->H2CEngines[i]->Running = TRUE;
                }
            }
            for (ULONG i = 0; i < deviceContext->C2HChannelMax; i++) {
                if (deviceContext->C2HEngines[i]) {
                    WRITE_REGISTER_ULONG((PULONG)&deviceContext->C2HEngines[i]->Regs->control,
                                       XDMA_CTRL_RUN_STOP);
                    deviceContext->C2HEngines[i]->Running = TRUE;
                }
            }
        }
        break;

    case IOCTL_XDMA_PERFORM_DMA:
        {
            if (InputBufferLength < sizeof(XDMA_DMA_TRANSFER)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PXDMA_DMA_TRANSFER transfer = (PXDMA_DMA_TRANSFER)inputBuffer;
            PXDMA_ENGINE engine;

            // Validate parameters
            if (transfer->Direction == 0) { // H2C
                if (transfer->Channel >= deviceContext->H2CChannelMax) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                engine = deviceContext->H2CEngines[transfer->Channel];
            } else { // C2H
                if (transfer->Channel >= deviceContext->C2HChannelMax) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                engine = deviceContext->C2HEngines[transfer->Channel];
            }

            if (!engine) {
                status = STATUS_DEVICE_NOT_READY;
                break;
            }

            // Create and submit DMA transfer
            WDFMEMORY memory;
            status = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES,
                                               (PVOID)transfer->LocalAddress,
                                               transfer->Length,
                                               &memory);
            if (!NT_SUCCESS(status)) {
                break;
            }

            PXDMA_TRANSFER xdmaTransfer;
            status = XdmaTransferCreate(engine,
                                      memory,
                                      transfer->Length,
                                      transfer->Direction == 0,
                                      transfer->IsCyclic,
                                      NULL, // No callback for now
                                      NULL,
                                      &xdmaTransfer);
            if (!NT_SUCCESS(status)) {
                WdfObjectDelete(memory);
                break;
            }

            xdmaTransfer->Offset = transfer->RemoteAddress;
            status = XdmaTransferSubmit(engine, xdmaTransfer);
            if (!NT_SUCCESS(status)) {
                XdmaTransferDestroy(xdmaTransfer);
                WdfObjectDelete(memory);
                break;
            }
        }
        break;

    case IOCTL_XDMA_ADDRMODE_GET:
        {
            if (OutputBufferLength < sizeof(XDMA_ADDRMODE)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PXDMA_ADDRMODE addrMode = (PXDMA_ADDRMODE)outputBuffer;
            PXDMA_ENGINE engine;

            // Get engine based on channel
            if (addrMode->Channel < deviceContext->H2CChannelMax) {
                engine = deviceContext->H2CEngines[addrMode->Channel];
            } else if (addrMode->Channel < deviceContext->C2HChannelMax) {
                engine = deviceContext->C2HEngines[addrMode->Channel];
            } else {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (!engine) {
                status = STATUS_DEVICE_NOT_READY;
                break;
            }

            addrMode->NonIncrementing = engine->NonIncrementingAddr;
            bytesReturned = sizeof(XDMA_ADDRMODE);
        }
        break;

    case IOCTL_XDMA_ADDRMODE_SET:
        {
            if (InputBufferLength < sizeof(XDMA_ADDRMODE)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PXDMA_ADDRMODE addrMode = (PXDMA_ADDRMODE)inputBuffer;
            PXDMA_ENGINE engine;

            // Get engine based on channel
            if (addrMode->Channel < deviceContext->H2CChannelMax) {
                engine = deviceContext->H2CEngines[addrMode->Channel];
            } else if (addrMode->Channel < deviceContext->C2HChannelMax) {
                engine = deviceContext->C2HEngines[addrMode->Channel];
            } else {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (!engine) {
                status = STATUS_DEVICE_NOT_READY;
                break;
            }

            engine->NonIncrementingAddr = addrMode->NonIncrementing;
        }
        break;

    case IOCTL_XDMA_PERFORMANCE_MEASURE:
        {
            if (OutputBufferLength < sizeof(XDMA_PERF_DATA)) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PXDMA_PERF_DATA perfData = (PXDMA_PERF_DATA)outputBuffer;
            PXDMA_ENGINE engine;

            // Get engine based on direction and channel
            if (perfData->Direction == 0) { // H2C
                if (perfData->Channel >= deviceContext->H2CChannelMax) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                engine = deviceContext->H2CEngines[perfData->Channel];
            } else { // C2H
                if (perfData->Channel >= deviceContext->C2HChannelMax) {
                    status = STATUS_INVALID_PARAMETER;
                    break;
                }
                engine = deviceContext->C2HEngines[perfData->Channel];
            }

            if (!engine) {
                status = STATUS_DEVICE_NOT_READY;
                break;
            }

            // Calculate performance metrics
            perfData->TotalBytes = engine->BytesProcessed;
            perfData->TotalTime = engine->TotalTime;
            if (perfData->TotalTime > 0) {
                perfData->Throughput = (perfData->TotalBytes * 1000000ULL) / 
                                     (perfData->TotalTime * 1024 * 1024); // MB/s
            } else {
                perfData->Throughput = 0;
            }
            bytesReturned = sizeof(XDMA_PERF_DATA);
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

exit:
    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}
