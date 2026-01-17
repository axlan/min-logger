#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

#ifndef MIN_LOGGER_DISABLE_VERBOSE_LOGGING
    #define MIN_LOGGER_DISABLE_VERBOSE_LOGGING false
#endif

#ifndef MIN_LOGGER_VERBOSE_LOGGING_BY_DEFAULT
    #define MIN_LOGGER_VERBOSE_LOGGING_BY_DEFAULT false
#endif

#if MIN_LOGGER_DISABLE_VERBOSE_LOGGING && MIN_LOGGER_VERBOSE_LOGGING_BY_DEFAULT
    #error Can't enable verbose logging if it's compiled out. Set "MIN_LOGGER_DISABLE_VERBOSE_LOGGING=0".
#endif

#ifndef MIN_LOGGER_LOG_INTERNAL_LIB
    #define MIN_LOGGER_LOG_INTERNAL_LIB false
#endif

#if MIN_LOGGER_ENABLED

/// A single point in time, measured in nanoseconds since the Unix epoch.
typedef int64_t time_point_value_t;


uint64_t min_logger_get_time_nanoseconds();
void min_logger_get_thread_name(char* thread_name);

void min_logger_write_msg_from_id(const char* log_msg_id, const char* payload, size_t payload_len);
void min_logger_format_and_write_log(const char** tags, const char* file_name,
                                        unsigned int line_number,
                                        // Could possibly include function name, though this is more
                                        // difficult to generate so isn't going to be included.
                                        const char* msg, int severity);
void min_logger_write_thread_names();

bool* min_logging_is_verbose();

    #if !MIN_LOGGER_LOG_INTERNAL_LIB
        #include "_min_logger_gen.h"  // NOLINT
    #endif

    // This macro is extracted by python/src/min_logger/builder.py which handles it differently from
    // the actual C preprocessor. The level must either be an integer, or one of priority level
    // names (INFO, WARN, etc.). IT CANNOT BE A VARIABLE OR MACRO. The `msg` must be a string
    // literal declared in place. IT CANNOT BE A VARIABLE. IT CANNOT BE A VARIABLE OR MACRO.
    #define MIN_LOGGER_LOG(level, msg, id) \
        if (MIN_LOGGER_MIN_LEVEL >= level) { \
            min_logger_log_func_##id(); \
        }

    #define MIN_LOGGER_RECORD_U64(level, value, id) \
        if (MIN_LOGGER_MIN_LEVEL >= level) { \
        }

    #define MIN_LOGGER_RECORD_STRING(level, value, id) \
        if (MIN_LOGGER_MIN_LEVEL >= level) { \
        }

    #define MIN_LOGGER_ENTER(level, id) \
        if (MIN_LOGGER_MIN_LEVEL >= level) { \
        }

    #define MIN_LOGGER_EXIT(level, id) \
        if (MIN_LOGGER_MIN_LEVEL >= level) { \
        }

#else
    #error "NEED TO IMPLEMENT!"
#endif

#ifdef __cplusplus
}
#endif
