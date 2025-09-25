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

// Weak functions to override for custom formatting/serialization.
void min_logger_write_binary_msg_from_id(MinLoggerCRC msg_id, const uint8_t* payload,
                                         size_t payload_len);
void min_logger_write_str_msg_from_id(MinLoggerCRC msg_id, const char* payload, size_t payload_len);
void min_logger_format_and_write_log(const char* file_name, unsigned int line_number,
                                     // Could possibly include function name, though this is more
                                     // difficult to generate so isn't going to be included.
                                     const char* msg, int severity);

// Thread safe function for requesting threads report their names.
void min_logger_write_thread_names();

// Thread unsafe functions for configuring logger at runtime.
bool* min_logger_is_verbose();
bool* min_logger_is_binary();
int* min_logger_level();

void min_logger_write_msg_from_id(MinLoggerCRC msg_id, const void* payload, size_t payload_len);

    // This macro is extracted by python/src/min_logger/builder.py which handles it differently from
    // the actual C preprocessor. The level must either be an integer, or one of priority level
    // names (INFO, WARN, etc.). IT CANNOT BE A VARIABLE OR MACRO. The `msg` must be a string
    // literal declared in place. IT CANNOT BE A VARIABLE. IT CANNOT BE A VARIABLE OR MACRO.
    #define MIN_LOGGER_LOG_ID(id, level, msg)                                      \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) {       \
            if (!MIN_LOGGER_DISABLE_VERBOSE_LOGGING && *min_logger_is_verbose()) { \
                min_logger_format_and_write_log(__FILE__, __LINE__, msg, level);   \
            } else {                                                               \
                min_logger_write_msg_from_id(id, NULL, 0);                         \
            }                                                                      \
        }

    #define MIN_LOGGER_LOG(level, msg)                     \
        {                                                  \
            __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC); \
            MIN_LOGGER_LOG_ID(__min_log_id, level, msg);   \
        }

    #define MIN_LOGGER_RECORD_U64_ID(id, level, name, value)                                 \
        __MIN_LOGGER_ASSERT_TYPE(value, uint64_t);                                           \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) {                 \
            if (!MIN_LOGGER_DISABLE_VERBOSE_LOGGING && *min_logger_is_verbose()) {           \
                char __tmp_min_logger_buffer[64];                                            \
                snprintf(__tmp_min_logger_buffer, 64, "%s: %" PRIu64, name, value);          \
                min_logger_format_and_write_log(__FILE__, __LINE__, __tmp_min_logger_buffer, \
                                                level);                                      \
            } else if (*min_logger_is_binary()) {                                            \
                min_logger_write_msg_from_id(id, &value, sizeof(uint64_t));                  \
            } else {                                                                         \
                char __tmp_min_logger_buffer[33];                                            \
                snprintf(__tmp_min_logger_buffer, 33, "%" PRIu64, value);                    \
                min_logger_write_msg_from_id(id, __tmp_min_logger_buffer,                    \
                                             strlen(__tmp_min_logger_buffer));               \
            }                                                                                \
        }

    #define MIN_LOGGER_RECORD_U64(level, name, value)                   \
        {                                                               \
            __MIN_LOGGER_LOG_MSG_GEN_ID(__MIN_LOGGER_LOC);              \
            MIN_LOGGER_RECORD_U64_ID(__min_log_id, level, name, value); \
        }

    #define MIN_LOGGER_RECORD_STRING_ID(id, level, name, value)                              \
        if (MIN_LOGGER_MIN_LEVEL >= level && *min_logger_level() >= level) {                 \
            if (!MIN_LOGGER_DISABLE_VERBOSE_LOGGING && *min_logger_is_verbose()) {           \
                char __tmp_min_logger_buffer[128];                                           \
                snprintf(__tmp_min_logger_buffer, 128, "%s: %s", name, value);               \
                min_logger_format_and_write_log(__FILE__, __LINE__, __tmp_min_logger_buffer, \
                                                level);                                      \
            } else {                                                                         \
                min_logger_write_msg_from_id(id, value, strlen(value));                      \
            }                                                                                \
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
