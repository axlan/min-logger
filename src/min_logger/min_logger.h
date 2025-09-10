#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIN_LOGGER_DEBUG 10
#define MIN_LOGGER_INFO 20
#define MIN_LOGGER_WARN 30
#define MIN_LOGGER_ERROR 40
#define MIN_LOGGER_CRITICAL 50

#ifndef MIN_LOGGER_ENABLED
    #define MIN_LOGGER_ENABLED 1
#endif

#ifndef MIN_LOGGER_MIN_LEVEL
    #define MIN_LOGGER_MIN_LEVEL (MIN_LOGGER_INFO)
#endif

#ifndef MIN_LOGGER_LOG_INTERNAL_LIB
    #define MIN_LOGGER_LOG_INTERNAL_LIB false
#endif

#if MIN_LOGGER_ENABLED

/// A single point in time, measured in nanoseconds since the Unix epoch.
typedef int64_t time_point_value_t;

extern const char** MIN_LOGGER_NO_TAGS;

extern void min_logger_write(const char* msg);
extern void min_logger_format_and_write(const char** tags, const char* file_name,
                                        unsigned int line_number,
                                        // Could possibly include function name, though this is more
                                        // difficult to generate so isn't going to be included.
                                        const char* msg, int severity,
                                        time_point_value_t timestamp);

    #if !MIN_LOGGER_LOG_INTERNAL_LIB
        #include "_min_logger_gen.h"  // NOLINT
    #endif

    // This macro is extracted by python/src/min_logger/builder.py which handles it differently from
    // the actual C preprocessor. The level must either be an integer, or one of priority level
    // names (INFO, WARN, etc.). IT CANNOT BE A VARIABLE OR MACRO. The `msg` must be a string
    // literal declared in place. IT CANNOT BE A VARIABLE. IT CANNOT BE A VARIABLE OR MACRO. The
    // __VA_ARGS__ are used to specify the tags for the log line. Also, the macro must be declared
    // as a single line.
    #define MIN_LOGGER_LOG(level, msg, id, ...) min_logger_func_##id()

#else
    #error "NEED TO IMPLEMENT!"
#endif

#ifdef __cplusplus
}
#endif
