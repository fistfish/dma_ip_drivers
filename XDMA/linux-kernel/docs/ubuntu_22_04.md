# XDMA Driver Ubuntu 22.04 Compatibility Guide

## Overview
This document describes the changes made to support the XDMA driver on Ubuntu 22.04 (kernel 5.15+) while maintaining compatibility with older kernel versions.

## Version Compatibility
- Minimum supported kernel: 3.10
- Added support for Ubuntu 22.04 (kernel 5.15+)
- Version-specific code handling for kernels 5.15 and 5.16

## Major Changes

### 1. Kernel Version Detection
The build system now includes proper detection of kernel versions to enable/disable features:
```makefile
# Kernel version detection in Makefile
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
KVER := $(shell echo $(KERNELDIR) | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
KMAJ := $(shell echo $(KVER) | cut -d'.' -f1)
KMIN := $(shell echo $(KVER) | cut -d'.' -f2)
```

### 2. IRQ Handling Updates
- Added IRQF_ONESHOT flag support for MSI-X interrupts on kernel 5.15+
- Updated PCI IRQ allocation with PCI_IRQ_AFFINITY support
- Modified interrupt handler registration for improved compatibility

### 3. DMA API Changes
- Updated DMA mask and coherent DMA handling
- Maintained compatibility with both new and legacy DMA APIs
- Enhanced error handling for DMA operations

### 4. Completion Callback Updates
Support for different ki_complete signatures across kernel versions:
```c
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 16, 0)
    /* Single result with error handling */
    caio->iocb->ki_complete(caio->iocb, caio->err_cnt ? res2 : res);
#else
    /* Legacy - separate result and error values */
    caio->iocb->ki_complete(caio->iocb, res, res2);
#endif
```

## Build Requirements

### System Requirements
- Ubuntu 22.04 or compatible system
- Linux kernel headers (5.15+)
- Build essential tools (gcc, make)

### Building the Driver
1. Install required packages:
```bash
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r)
```

2. Build the driver:
```bash
cd XDMA/linux-kernel/xdma
make clean
make KERNELDIR=/lib/modules/$(uname -r)/build
```

3. Install the driver:
```bash
sudo make install
```

## Testing
To verify the driver installation:
1. Load the module:
```bash
sudo modprobe xdma
```

2. Check driver status:
```bash
dmesg | grep xdma
lsmod | grep xdma
```

## Known Issues and Limitations
1. BTF generation may be skipped if vmlinux is not available (non-critical)
2. Must build against matching kernel headers for the target system

## Version History
- 2020.2.3: Added Ubuntu 22.04 support
  - Enhanced kernel version detection
  - Updated IRQ handling
  - Improved DMA API compatibility
