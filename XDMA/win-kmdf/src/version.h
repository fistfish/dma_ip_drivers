/*++
Module Name:
    version.h

Abstract:
    Version information for the XDMA Windows driver
    Matches the Linux driver version numbering
--*/

#ifndef __XDMA_VERSION_H__
#define __XDMA_VERSION_H__

// Version numbers from Linux driver
#define DRV_MOD_MAJOR        2020
#define DRV_MOD_MINOR        2
#define DRV_MOD_PATCHLEVEL   3

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define DRV_MODULE_VERSION      \
    STR(DRV_MOD_MAJOR) "." \
    STR(DRV_MOD_MINOR) "." \
    STR(DRV_MOD_PATCHLEVEL)

#endif // __XDMA_VERSION_H__
