#pragma once

#include <inttypes.h>  // Required for PRIu64
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t MinLoggerCRC;

#ifdef __cplusplus
    #include "min_logger_crc.h"

    #define __MIN_LOGGER_LOG_MSG_GEN_ID(str) \
        static constexpr MinLoggerCRC __min_log_id = MIN_LOGGER_CPP_CRC32(str);

    #define __MIN_LOGGER_ASSERT_TYPE(x, expected_type)                 \
        static_assert(std::is_same<decltype(x), expected_type>::value, \
                      "Type mismatch: expected " #expected_type)

extern "C" {
#else  // is compiling a C not C++
MinLoggerCRC MIN_LOGGER_C_CRC32(const char* str);
    #define __MIN_LOGGER_LOG_MSG_GEN_ID(str)                     \
        static MinLoggerCRC __min_log_id = 0;                    \
        if (__min_log_id == 0) {                                 \
            __min_log_id = MIN_LOGGER_C_CRC32(__MIN_LOGGER_LOC); \
        }

    #define __MIN_LOGGER_ASSERT_TYPE(x, expected_type)              \
        _Static_assert(_Generic((x), expected_type: 1, default: 0), \
                       "Type mismatch: expected " #expected_type)
#endif

typedef enum {
    MIN_LOGGER_PAYLOAD_NONE,
    MIN_LOGGER_PAYLOAD_STRING,
    MIN_LOGGER_PAYLOAD_U64
} PayloadType;
typedef void (*MinLoggerSerializeCallBack)(MinLoggerCRC msg_id, PayloadType payload_type,
                                           const void* payload, size_t payload_len);

typedef void (*MinLoggerVerboseCallBack)(MinLoggerCRC msg_id, const char* file_name,
                                         unsigned int line_number, const char* function_name,
                                         const char* msg, int severity, PayloadType payload_type,
                                         const void* payload, size_t payload_len);

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

#ifndef MIN_LOGGER_DEFAULT_VERBOSE
    #define MIN_LOGGER_DEFAULT_VERBOSE false
#endif

#ifndef MIN_LOGGER_DEFAULT_BINARY
    #define MIN_LOGGER_DEFAULT_BINARY false
#endif

#ifndef MIN_LOGGER_DEFAULT_LEVEL
    #define MIN_LOGGER_DEFAULT_LEVEL MIN_LOGGER_WARN
#endif

#if MIN_LOGGER_DISABLE_VERBOSE_LOGGING && MIN_LOGGER_DEFAULT_VERBOSE
    #error \
        "Can't enable verbose logging if it's compiled out. Change MIN_LOGGER_DISABLE_VERBOSE_LOGGING or MIN_LOGGER_DEFAULT_VERBOSE."
#endif

#if MIN_LOGGER_DEFAULT_BINARY && MIN_LOGGER_DEFAULT_VERBOSE
    #error \
        "Can't enable verbose and binary logging simultaneously. Change MIN_LOGGER_DISABLE_VERBOSE_LOGGING or MIN_LOGGER_DEFAULT_BINARY."
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

// Thread unsafe functions for configuring logger at runtime.
bool* min_logger_is_verbose();
// Defaults to MIN_LOGGER_DEFAULT_VERBOSE_FORMAT
MinLoggerVerboseCallBack* min_logger_verbose_format();
// Defaults to MIN_LOGGER_DEFAULT_TEXT_SERIALIZED_FORMAT
MinLoggerSerializeCallBack* min_logger_serialize_format();
int* min_logger_level();

// Built in formats
extern const MinLoggerVerboseCallBack MIN_LOGGER_DEFAULT_VERBOSE_FORMAT;
extern const MinLoggerSerializeCallBack MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT;
extern const MinLoggerSerializeCallBack MIN_LOGGER_DEFAULT_TEXT_SERIALIZED_FORMAT;

    // This macro is extracted by python/src/min_logger/builder.py which handles it differently from
    // the actual C preprocessor. The level must either be an integer, or one of priority level
    // names (INFO, WARN, etc.). IT CANNOT BE A VARIABLE OR MACRO. The `msg` must be a string
    // literal declared in place. IT CANNOT BE A VARIABLE. IT CANNOT BE A VARIABLE OR MACRO.
    #define MIN_LOGGER_LOG_ID(id, level, msg)                                                \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) {                 \
            if (!MIN_LOGGER_DISABLE_VERBOSE_LOGGING && *min_logger_is_verbose()) {           \
                (*min_logger_verbose_format())(id, __FILE__, __LINE__, __func__, msg, level, \
                                               MIN_LOGGER_PAYLOAD_NONE, NULL, 0);            \
            } else {                                                                         \
                (*min_logger_serialize_format())(id, MIN_LOGGER_PAYLOAD_NONE, NULL, 0);      \
            }                                                                                \
        }

    #define MIN_LOGGER_LOG(level, msg)                     \
        {                                                  \
            __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC); \
            MIN_LOGGER_LOG_ID(__min_log_id, level, msg);   \
        }

    #define MIN_LOGGER_RECORD_VALUE_ID(id, level, name, type, value)                          \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) {                  \
            if (!MIN_LOGGER_DISABLE_VERBOSE_LOGGING && *min_logger_is_verbose()) {            \
                (*min_logger_verbose_format())(id, __FILE__, __LINE__, __func__, name, level, \
                                               type, &value, sizeof(value));                  \
            } else {                                                                          \
                (*min_logger_serialize_format())(id, type, &value, sizeof(value));            \
            }                                                                                 \
        }

    #define MIN_LOGGER_RECORD_VALUE(level, name, type, value)                   \
        {                                                                       \
            __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC);                      \
            MIN_LOGGER_RECORD_VALUE_ID(__min_log_id, level, name, type, value); \
        }

    #define MIN_LOGGER_RECORD_STRING_ID(id, level, name, value)                                  \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) {                     \
            if (!MIN_LOGGER_DISABLE_VERBOSE_LOGGING && *min_logger_is_verbose()) {               \
                (*min_logger_verbose_format())(id, __FILE__, __LINE__, __func__, name, level,    \
                                               MIN_LOGGER_PAYLOAD_STRING, value, strlen(value)); \
            } else {                                                                             \
                (*min_logger_serialize_format())(id, MIN_LOGGER_PAYLOAD_STRING, value,           \
                                                 strlen(value));                                 \
            }                                                                                    \
        }

    #define MIN_LOGGER_RECORD_STRING(level, name, value)                   \
        {                                                                  \
            __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC);                 \
            MIN_LOGGER_RECORD_STRING_ID(__min_log_id, level, name, value); \
        }

    #define MIN_LOGGER_ENTER_ID(level, name, id) MIN_LOGGER_LOG_ID(id, level, name "_enter")
    #define MIN_LOGGER_ENTER(level, name) MIN_LOGGER_LOG(level, name "_enter")

    #define MIN_LOGGER_EXIT_ID(level, name) MIN_LOGGER_LOG_ID(id, level, name "_exit")
    #define MIN_LOGGER_EXIT(level, name) MIN_LOGGER_LOG(level, name "_exit")

#else
    #error "NEED TO IMPLEMENT!"
#endif

#ifdef __cplusplus
}  // extern "C"
#endif
