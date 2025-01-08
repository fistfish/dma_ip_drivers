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

// Trace message format strings
#define TRACE_LEVEL_ERROR        0
#define TRACE_LEVEL_WARNING      1
#define TRACE_LEVEL_INFORMATION  2
#define TRACE_LEVEL_VERBOSE      3

// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// begin_wpp config
// FUNC Trace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// end_wpp
//
