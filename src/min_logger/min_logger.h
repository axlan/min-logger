#pragma once

#include <inttypes.h>  // Required for PRIu64
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t MinLoggerCRC;

#ifdef __cplusplus
    #include <type_traits>

    #include "min_logger_crc.h"

    #define __MIN_LOGGER_LOG_MSG_GEN_ID(str) \
        static constexpr MinLoggerCRC __min_log_id = MIN_LOGGER_CPP_CRC32(str);

    #define __MIN_LOGGER_ASSERT_TYPE(x, expected_type)                                   \
        static_assert(std::is_same<std::decay<decltype(x)>::type, expected_type>::value, \
                      "Type mismatch: expected " #expected_type)

extern "C" {
#else  // is compiling a C not C++
    #define __MIN_LOGGER_ASSERT_TYPE(x, expected_type)              \
        _Static_assert(_Generic((x), expected_type: 1, default: 0), \
                       "Type mismatch: expected " #expected_type)
#endif

typedef void (*MinLoggerSerializeCallBack)(MinLoggerCRC msg_id, const void* payload,
                                           size_t payload_len);

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

#ifndef MIN_LOGGER_DEFAULT_LEVEL
    #define MIN_LOGGER_DEFAULT_LEVEL MIN_LOGGER_WARN
#endif

#if MIN_LOGGER_ENABLED

    #define __MIN_LOGGER_S1(x) #x
    #define __MIN_LOGGER_S2(x) __MIN_LOGGER_S1(x)
    #define __MIN_LOGGER_LOC __FILE__ ":" __MIN_LOGGER_S2(__LINE__)

// Weak functions to override with platform specific implementations
uint64_t min_logger_get_time_nanoseconds();
size_t min_logger_get_thread_name(char* thread_name, size_t max_len);

// Thread safe function for requesting threads report their names.
void min_logger_write_thread_names();

// Defaults to MIN_LOGGER_DEFAULT_TEXT_SERIALIZED_FORMAT
MinLoggerSerializeCallBack* min_logger_serialize_format();
int* min_logger_level();

// Built in formats
extern const MinLoggerSerializeCallBack MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT;
extern const MinLoggerSerializeCallBack MIN_LOGGER_MICRO_BINARY_SERIALIZED_FORMAT;

    // This macro is extracted by python/src/min_logger/builder.py which handles it differently from
    // the actual C preprocessor. The level must either be an integer, or one of priority level
    // names (INFO, WARN, etc.). IT CANNOT BE A VARIABLE OR MACRO. The `msg` must be a string
    // literal declared in place. IT CANNOT BE A VARIABLE. IT CANNOT BE A VARIABLE OR MACRO.
    #define MIN_LOGGER_LOG_ID(id, level, msg)                                \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) { \
            (*min_logger_serialize_format())(id, NULL, 0);                   \
        }

    #define MIN_LOGGER_RECORD_VALUE_ID(id, level, name, type, value)         \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) { \
            __MIN_LOGGER_ASSERT_TYPE(value, type);                           \
            (*min_logger_serialize_format())(id, &value, sizeof(type));      \
        }

    #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ID(id, level, name, type, value, msg) \
        MIN_LOGGER_RECORD_VALUE_ID(id, level, name, type, value)

    #define MIN_LOGGER_RECORD_VALUE_ARRAY_ID(id, level, name, type, values, num_values) \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) {            \
            __MIN_LOGGER_ASSERT_TYPE(*values, type);                                    \
            (*min_logger_serialize_format())(id, values, sizeof(type) * num_values);    \
        }

    #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY_ID(id, level, name, type, values, num_values, \
                                                     msg)                                       \
        MIN_LOGGER_RECORD_VALUE_ARRAY_ID(id, level, name, type, values, num_values)

    #define MIN_LOGGER_ENTER_ID(level, name, id) MIN_LOGGER_LOG_ID(id, level, name "_enter")

    #define MIN_LOGGER_EXIT_ID(level, name) MIN_LOGGER_LOG_ID(id, level, name "_exit")

    #ifdef __cplusplus
        #define MIN_LOGGER_LOG(level, msg)                     \
            {                                                  \
                __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC); \
                MIN_LOGGER_LOG_ID(__min_log_id, level, msg);   \
            }

        #define MIN_LOGGER_RECORD_VALUE(level, name, type, value)                   \
            {                                                                       \
                __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC);                      \
                MIN_LOGGER_RECORD_VALUE_ID(__min_log_id, level, name, type, value); \
            }

        #define MIN_LOGGER_RECORD_AND_LOG_VALUE(level, name, type, value, msg) \
            MIN_LOGGER_RECORD_VALUE(level, name, type, value)

        #define MIN_LOGGER_RECORD_VALUE_ARRAY(level, name, type, values, num_values)      \
            {                                                                             \
                __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC);                            \
                MIN_LOGGER_RECORD_VALUE_ARRAY_ID(__min_log_id, level, name, type, values, \
                                                 num_values);                             \
            }

        #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY(level, name, type, values, num_values, msg) \
            MIN_LOGGER_RECORD_VALUE_ARRAY(level, name, type, values, num_values)

        #define MIN_LOGGER_ENTER(level, name) MIN_LOGGER_LOG(level, name "_enter")

        #define MIN_LOGGER_EXIT(level, name) MIN_LOGGER_LOG(level, name "_exit")
    #endif

#else
    #error "NEED TO IMPLEMENT!"
#endif

#ifdef __cplusplus
}  // extern "C"
#endif
