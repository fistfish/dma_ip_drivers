/*++
Module Name:
    Device.h

Abstract:
    Header file for the XDMA device implementation
--*/

#include <ntddk.h>
#include <wdf.h>

// Type definitions to match Linux driver
typedef ULONG u32;

// Constants from Linux driver
#define MAX_USER_IRQ          16
#define XDMA_CHANNEL_NUM_MAX  4
#define XDMA_BAR_NUM         6

// PCI Vendor and Device IDs (from Linux driver's pci_ids[])
#define XILINX_VENDOR_ID     0x10EE
#define XILINX_DEVICE_ID_1   0x9048
#define XILINX_DEVICE_ID_2   0x9044
#define AWS_VENDOR_ID        0x1D0F
#define XDMA_BAR_SIZE        (1 << 17)  // 128KB minimum

// Block IDs from Linux driver
#define IRQ_BLOCK_ID         0x1FC0U
#define CONFIG_BLOCK_ID      0x1000U

// Register offsets
#define XDMA_OFS_INT_CTRL    0x2000
#define XDMA_OFS_CONFIG      0x3000

// Register structures (matching Linux driver)
struct interrupt_regs {
    u32 identifier;
    u32 user_int_enable;
    u32 user_int_enable_w1s;
    u32 user_int_enable_w1c;
    u32 channel_int_enable;
    u32 channel_int_enable_w1s;
    u32 channel_int_enable_w1c;
    u32 reserved_1[9];
    u32 user_int_request;
    u32 channel_int_request;
    u32 user_int_pending;
    u32 channel_int_pending;
    u32 reserved_2[12];
};

struct config_regs {
    u32 identifier;
    u32 reserved[4095];
};

// Forward declarations
EVT_WDF_INTERRUPT_ISR XdmaEvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC XdmaEvtInterruptDpc;
EVT_WDF_INTERRUPT_ENABLE XdmaEvtInterruptEnable;
EVT_WDF_INTERRUPT_DISABLE XdmaEvtInterruptDisable;

// Interrupt context structure
typedef struct _XDMA_INTERRUPT_CONTEXT {
    PXDMA_DEVICE_CONTEXT DeviceContext;
    ULONG Vector;                // MSI-X vector number
    ULONG ChannelId;            // Channel ID for channel-specific interrupts
    BOOLEAN IsUserInterrupt;     // TRUE for user interrupts, FALSE for channel interrupts
} XDMA_INTERRUPT_CONTEXT, *PXDMA_INTERRUPT_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(XDMA_INTERRUPT_CONTEXT, XdmaGetInterruptContext)

// Device context structure (maps to xdma_pci_dev in Linux driver)
typedef struct _XDMA_DEVICE_CONTEXT
{
    WDFDEVICE       WdfDevice;
    WDFINTERRUPT    ChannelInterrupt[XDMA_CHANNEL_NUM_MAX * 2]; // H2C + C2H
    WDFINTERRUPT    UserInterrupt[MAX_USER_IRQ];
    WDFDMAENABLER   DmaEnabler;
    
    // PCIe resources
    PHYSICAL_ADDRESS BarBasePA[XDMA_BAR_NUM];
    PVOID           BarBaseVA[XDMA_BAR_NUM];
    SIZE_T          BarLength[XDMA_BAR_NUM];
    
    // Device configuration (matches Linux driver)
    ULONG           UserMax;
    ULONG           H2CChannelMax;
    ULONG           C2HChannelMax;
    
    // Bar indices (matches Linux driver)
    INT             UserBarIdx;
    INT             ConfigBarIdx;
    INT             BypassBarIdx;

    // Interrupt state
    BOOLEAN         MsixEnabled;
    BOOLEAN         MsiEnabled;
    
} XDMA_DEVICE_CONTEXT, *PXDMA_DEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(XDMA_DEVICE_CONTEXT, XdmaGetDeviceContext)

// Device event handlers
EVT_WDF_DEVICE_CONTEXT_CLEANUP XdmaEvtDeviceContextCleanup;
EVT_WDF_DEVICE_PREPARE_HARDWARE XdmaEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE XdmaEvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY XdmaEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT XdmaEvtDeviceD0Exit;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL XdmaEvtIoDeviceControl;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(XDMA_DEVICE_CONTEXT, XdmaGetDeviceContext)

// Device event handlers
EVT_WDF_DEVICE_PREPARE_HARDWARE XdmaEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE XdmaEvtDeviceReleaseHardware;
EVT_WDF_DEVICE_D0_ENTRY XdmaEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT XdmaEvtDeviceD0Exit;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL XdmaEvtIoDeviceControl;

// Device context fields for IOCTL handling
    USHORT VendorId;
    USHORT DeviceId;
    USHORT SubsystemVendorId;
    USHORT SubsystemId;
    ULONG DmaEngineVersion;
    ULONG64 FeatureId;
    USHORT Domain;
    UCHAR Bus;
    UCHAR Dev;
    UCHAR Func;

    // DMA engine arrays
    PXDMA_ENGINE H2CEngines[XDMA_CHANNEL_NUM_MAX];
    PXDMA_ENGINE C2HEngines[XDMA_CHANNEL_NUM_MAX];
    
    // Engine configuration
    ULONG EnginesNum;
    ULONG H2CChannelMax;
    ULONG C2HChannelMax;
    ULONG MaskIrqH2C;
    ULONG MaskIrqC2H;
