/*++
Module Name:
    DmaStructures.h

Abstract:
    DMA structures for XDMA Windows driver
    Maps Linux XDMA structures to Windows KMDF equivalents
--*/

#ifndef __XDMA_DMA_STRUCTURES_H__
#define __XDMA_DMA_STRUCTURES_H__

#include <ntddk.h>
#include <wdf.h>

// Forward declarations
typedef struct _XDMA_DEVICE_CONTEXT XDMA_DEVICE_CONTEXT, *PXDMA_DEVICE_CONTEXT;
typedef struct _XDMA_ENGINE XDMA_ENGINE, *PXDMA_ENGINE;
typedef struct _XDMA_TRANSFER XDMA_TRANSFER, *PXDMA_TRANSFER;
typedef struct _XDMA_RESULT XDMA_RESULT, *PXDMA_RESULT;

// Engine type information
typedef struct _WDF_XDMA_ENGINE_TYPE_INFO {
    ULONG UniqueType;
    ULONG Version;
    ULONG Features;
    ULONG Reserved[5];
} WDF_XDMA_ENGINE_TYPE_INFO, *PWDF_XDMA_ENGINE_TYPE_INFO;

// Cyclic transfer callback
typedef VOID (*PFN_XDMA_CYCLIC_CALLBACK)(
    PVOID Context,
    ULONG64 BytesTransferred,
    NTSTATUS Status
    );

// Maximum transfer size per descriptor
#define XDMA_DESC_BLEN_BITS   28
#define XDMA_DESC_BLEN_MAX    ((1 << (XDMA_DESC_BLEN_BITS)) - 1)

// DMA descriptor control bits (from Linux driver)
#define XDMA_DESC_STOPPED     (1UL << 0)
#define XDMA_DESC_COMPLETED   (1UL << 1)
#define XDMA_DESC_EOP         (1UL << 4)

// Engine control and interrupt enable bits
#define XDMA_CTRL_RUN_STOP              (1UL << 0)
#define XDMA_CTRL_IE_DESC_STOPPED       (1UL << 1)
#define XDMA_CTRL_IE_DESC_COMPLETED     (1UL << 2)
#define XDMA_CTRL_IE_DESC_ALIGN_MISMATCH (1UL << 3)
#define XDMA_CTRL_IE_MAGIC_STOPPED      (1UL << 4)
#define XDMA_CTRL_IE_IDLE_STOPPED       (1UL << 5)
#define XDMA_CTRL_IE_READ_ERROR         (1UL << 6)
#define XDMA_CTRL_IE_DESC_ERROR         (1UL << 7)

// Channel and SGDMA offsets (from Linux driver)
#define H2C_CHANNEL_OFFSET              0x1000
#define CHANNEL_SPACING                 0x100
#define SGDMA_OFFSET_FROM_CHANNEL       0x4000

// DMA descriptor structure (matches Linux xdma_desc)
typedef struct _XDMA_DESC {
    ULONG Control;
    ULONG Bytes;          // Transfer length in bytes
    ULONG SrcAddrLow;     // Source address (low 32-bit)
    ULONG SrcAddrHigh;    // Source address (high 32-bit)
    ULONG DstAddrLow;     // Destination address (low 32-bit)
    ULONG DstAddrHigh;    // Destination address (high 32-bit)
    ULONG NextLow;        // Next descriptor address (low 32-bit)
    ULONG NextHigh;       // Next descriptor address (high 32-bit)
} XDMA_DESC, *PXDMA_DESC;

// DMA transfer state (matches Linux transfer_state enum)
typedef enum _XDMA_TRANSFER_STATE {
    XFER_STATE_NEW = 0,
    XFER_STATE_SUBMITTED,
    XFER_STATE_COMPLETED,
    XFER_STATE_FAILED,
    XFER_STATE_ABORTED
} XDMA_TRANSFER_STATE;

// DMA transfer structure (maps to Linux xdma_transfer)
typedef struct _XDMA_TRANSFER {
    LIST_ENTRY TransferList;           // Queue of non-completed transfers
    PXDMA_DESC DescriptorVirt;        // Virtual address of first descriptor
    PHYSICAL_ADDRESS DescriptorPhys;   // Physical address of first descriptor
    ULONG DescriptorCount;            // Number of descriptors in transfer
    ULONG DescriptorCompleted;        // Completed descriptors
    XDMA_TRANSFER_STATE State;        // State of the transfer
    BOOLEAN IsCyclic;                 // Flag if transfer is cyclic
    WDFDMATRANSACTION DmaTransaction; // Associated DMA transaction
    WDFCOMMONBUFFER CommonBuffer;     // Common buffer for descriptors
    PVOID CommonBufferVirt;           // Virtual address of common buffer
    PHYSICAL_ADDRESS CommonBufferPhys; // Physical address of common buffer
    size_t Length;                    // Total transfer length
    PXDMA_ENGINE Engine;             // Parent engine
    ULONG64 Offset;                  // Device memory offset
    ULONG64 BytesTransferred;        // Number of bytes transferred
    
    // Performance monitoring
    LARGE_INTEGER StartTime;         // Transfer start time
    ULONG64 TotalBytes;             // Total bytes transferred
    ULONG64 TotalTime;              // Total transfer time in microseconds
    
    // Cyclic transfer
    BOOLEAN CyclicCallback;         // TRUE if cyclic callback is enabled
    PVOID CyclicContext;           // Context for cyclic callback
    PFN_XDMA_CYCLIC_CALLBACK CyclicCallbackFn; // Cyclic transfer callback
} XDMA_TRANSFER, *PXDMA_TRANSFER;

// Engine register definitions (from Linux driver)
typedef struct _XDMA_ENGINE_REGS {
    ULONG Identifier;
    ULONG Control;
    ULONG ControlW1S;
    ULONG ControlW1C;
    ULONG Status;
    ULONG StatusRC;
    ULONG CompletedDescCount;
    ULONG AlignmentsRegs[5];
    ULONG PollModeWbLo;
    ULONG PollModeWbHi;
    ULONG InterruptEnableMask;
    ULONG InterruptEnableMaskW1S;
    ULONG InterruptEnableMaskW1C;
    ULONG Reserved_0x50[4];
    ULONG PerformanceCtrl;
    ULONG Reserved_0x64;
    ULONG PerformanceCycHi;
    ULONG PerformanceCycLo;
    ULONG PerformanceDatHi;
    ULONG PerformanceDatLo;
    ULONG PerformancePndHi;
    ULONG PerformancePndLo;
    
    // Additional fields for interrupt handling
    ULONG interrupt_enable_mask;    // Current interrupt enable mask
} XDMA_ENGINE_REGS, *PXDMA_ENGINE_REGS;

typedef struct _XDMA_ENGINE_SGDMA_REGS {
    ULONG Identifier;
    ULONG Reserved_0x04[3];
    ULONG FirstDescLo;
    ULONG FirstDescHi;
    ULONG FirstDescAdj;
    ULONG Reserved_0x1C;
    ULONG CreditsModeEnable;
} XDMA_ENGINE_SGDMA_REGS, *PXDMA_ENGINE_SGDMA_REGS;

// DMA engine structure (maps to Linux xdma_engine)
typedef struct _XDMA_ENGINE {
    ULONG Magic;                    // Magic number for validation
    struct _XDMA_DEVICE_CONTEXT *DeviceContext; // Parent device
    WDFSPINLOCK Lock;              // Protects concurrent access
    CHAR Name[32];                 // Name of this engine
    
    // Engine configuration
    BOOLEAN IsH2C;                 // TRUE for H2C, FALSE for C2H
    ULONG Channel;                 // Channel number
    BOOLEAN Streaming;             // Streaming vs MM mode
    BOOLEAN Running;               // Engine is running
    
    // Register access
    PXDMA_ENGINE_REGS Regs;       // Control registers
    PXDMA_ENGINE_SGDMA_REGS SgdmaRegs; // SGDMA registers
    
    // Interrupt handling
    ULONG IrqBitmask;             // Interrupt bitmask
    ULONG InterruptEnableMaskValue; // Current interrupt mask
    
    // DMA resources
    WDFDMAENABLER DmaEnabler;     // WDF DMA enabler
    ULONG DescMax;                // Maximum descriptors
    PXDMA_DESC DescriptorRing;    // Ring buffer of descriptors
    PHYSICAL_ADDRESS DescriptorRingPhys; // Physical address of ring
    WDFCOMMONBUFFER DescriptorBuffer; // Common buffer for descriptors
    LIST_ENTRY TransferQueue;      // Queue of pending transfers
    SIZE_T DescriptorRingSize;    // Size of descriptor ring buffer
    
    // Device memory configuration
    SIZE_T DeviceSize;           // Size of device memory region
    PHYSICAL_ADDRESS DeviceAddress; // Physical address of device memory
    BOOLEAN NonIncrementingAddr;  // Non-incrementing address mode
    
    // Cyclic transfer support
    PXDMA_RESULT CyclicResult;    // Cyclic result buffer
    PHYSICAL_ADDRESS CyclicResultPhys; // Physical address of result buffer
    WDFCOMMONBUFFER CyclicResultBuffer; // Common buffer for results
    
    // Performance monitoring
    ULONG64 BytesProcessed;       // Total bytes processed
    ULONG64 TotalTime;           // Total processing time
    
    // Engine type information
    WDF_XDMA_ENGINE_TYPE_INFO TypeInfo; // Engine type and capabilities
    
    // Bypass mode
    ULONG BypassOffset;          // Bypass mode offset
} XDMA_ENGINE, *PXDMA_ENGINE;

// Function declarations
NTSTATUS
XdmaEngineCreate(
    WDFDEVICE Device,
    PXDMA_DEVICE_CONTEXT DeviceContext,
    BOOLEAN IsH2C,
    ULONG Channel,
    PXDMA_ENGINE *Engine
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

NTSTATUS
XdmaTransferCreate(
    PXDMA_ENGINE Engine,
    WDFMEMORY Memory,
    size_t Length,
    BOOLEAN WriteToDevice,
    PXDMA_TRANSFER *Transfer
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

#endif // __XDMA_DMA_STRUCTURES_H__
