/*++
Module Name:
    Trace.h

Abstract:
    Header file for the debug tracing
--*/

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
