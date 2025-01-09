/*++
Module Name:
    Trace.h

Abstract:
    Header file for the debug tracing
--*/

#pragma once

// Verify that the WPP_CONTROL_GUIDS matches the GUID in the .tpl file
#pragma prefast(suppress:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID( \
        XdmaTraceGuid, (D5AC7068,C701,4AA9,A7A5,9CDC2C4DB7A4), \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO) \
        WPP_DEFINE_BIT(TRACE_DRIVER) \
        WPP_DEFINE_BIT(TRACE_DEVICE) \
        WPP_DEFINE_BIT(TRACE_DMA) \
    )

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) \
    WPP_LEVEL_LOGGER(flags)

#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) \
    (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

// XDMA-specific trace levels with unique names to avoid conflicts with WPP
#define XDMA_ERR_LEVEL        0  // Maps to WPP error level
#define XDMA_WARN_LEVEL      1   // Maps to WPP warning level
#define XDMA_INFO_LEVEL      2   // Maps to WPP information level
#define XDMA_DBG_LEVEL       3   // Maps to WPP verbose level

// Mapping macros to maintain compatibility with existing code
// XDMA-specific trace level mappings
#define XDMA_TRACE_ERROR     XDMA_ERR_LEVEL
#define XDMA_TRACE_WARNING   XDMA_WARN_LEVEL
#define XDMA_TRACE_INFO      XDMA_INFO_LEVEL
#define XDMA_TRACE_VERBOSE   XDMA_DBG_LEVEL

// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// begin_wpp config
// FUNC XdmaTrace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC XdmaTraceEvents(LEVEL, FLAGS, MSG, ...);
// end_wpp
//
