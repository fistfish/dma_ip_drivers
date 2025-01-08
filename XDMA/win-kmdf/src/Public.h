/*++
Module Name:
    Public.h

Abstract:
    Public interface definitions for the XDMA driver
--*/

#define XDMA_DEVICE_TYPE 0x8000

// Device interface GUID
// {D5AC7068-C701-4AA9-A7A5-9CDC2C4DB7A4}
DEFINE_GUID(GUID_DEVINTERFACE_XDMA,
    0xd5ac7068, 0xc701, 0x4aa9, 0xa7, 0xa5, 0x9c, 0xdc, 0x2c, 0x4d, 0xb7, 0xa4);

// IOCTLs mapped from Linux driver
#define IOCTL_XDMA_GET_VERSION \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_IOCINFO \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_IOCOFFLINE \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_IOCONLINE \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_PERFORM_DMA \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_APERTURE_REG \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_ADDRMODE_GET \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_ADDRMODE_SET \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_XDMA_PERFORMANCE_MEASURE \
    CTL_CODE(XDMA_DEVICE_TYPE, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Common structures shared between user mode and kernel mode
#define XDMA_MAGIC 0xAD4B0000  // Magic number for IOCTLs

typedef struct _XDMA_IOC_BASE {
    ULONG Magic;      // Must be XDMA_MAGIC
    ULONG Command;    // Command specific to the IOCTL
} XDMA_IOC_BASE, *PXDMA_IOC_BASE;

typedef struct _XDMA_VERSION_INFO {
    ULONG Major;
    ULONG Minor;
    ULONG Build;
} XDMA_VERSION_INFO, *PXDMA_VERSION_INFO;

typedef struct _XDMA_IOC_INFO {
    XDMA_IOC_BASE Base;
    USHORT Vendor;
    USHORT Device;
    USHORT SubsystemVendor;
    USHORT SubsystemDevice;
    ULONG DmaEngineVersion;
    ULONG DriverVersion;
    ULONG64 FeatureId;
    USHORT Domain;
    UCHAR Bus;
    UCHAR Dev;
    UCHAR Func;
} XDMA_IOC_INFO, *PXDMA_IOC_INFO;

typedef struct _XDMA_DMA_TRANSFER {
    XDMA_IOC_BASE Base;
    ULONG Channel;
    ULONG Direction;  // 0 = H2C, 1 = C2H
    ULONG64 LocalAddress;
    ULONG64 RemoteAddress;
    ULONG Length;
    ULONG Flags;
    BOOLEAN IsCyclic;
} XDMA_DMA_TRANSFER, *PXDMA_DMA_TRANSFER;

typedef struct _XDMA_APERTURE_REG {
    XDMA_IOC_BASE Base;
    ULONG64 Offset;
    ULONG Length;
    ULONG Direction;  // 0 = H2C, 1 = C2H
} XDMA_APERTURE_REG, *PXDMA_APERTURE_REG;

typedef struct _XDMA_ADDRMODE {
    XDMA_IOC_BASE Base;
    ULONG Channel;
    BOOLEAN NonIncrementing;
} XDMA_ADDRMODE, *PXDMA_ADDRMODE;

typedef struct _XDMA_PERF_DATA {
    XDMA_IOC_BASE Base;
    ULONG Channel;
    ULONG Direction;
    ULONG64 TotalBytes;
    ULONG64 TotalTime;    // In microseconds
    ULONG64 Throughput;   // In MB/s
} XDMA_PERF_DATA, *PXDMA_PERF_DATA;
