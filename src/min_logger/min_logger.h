/*
 * min-logger: A minimal, efficient, and portable logging library for embedded systems.
 *
 * This file defines the public API for the min-logger library. It provides macros that call a
 * serialization function with a compile-time generated ID. This ID is mapped to log message
 * context (file, line, message text) by external tools. This allows rich logging capabilities
 * with minimal runtime overhead, storage, and bandwidth.
 *
 * Key Features:
 * - Zero-overhead when disabled (MIN_LOGGER_ENABLED = 0)
 * - (C++) Compile-time ID generation based on source location
 * - Support for logging fixed-size values and variable-length arrays
 * - Platform-agnostic with customizable serialization format
 *
 * The two built in serialization formats report timestamps and thread names.
 * See min_logger.cpp for their details.
 *
 * Basic Usage (C++):
 *   MIN_LOGGER_LOG(MIN_LOGGER_INFO, "Application started");
 *   MIN_LOGGER_RECORD_VALUE(MIN_LOGGER_WARN, "sensor_reading", float, temperature);
 *
 * Basic Usage (C with explicit IDs):
 *   MIN_LOGGER_LOG_ID(0x12345678, MIN_LOGGER_INFO, "Application started");
 *   MIN_LOGGER_RECORD_VALUE_ID(0x12345679, MIN_LOGGER_WARN, "sensor_reading", float, temperature);
 *
 * See python/src/min_logger/builder_main.py for tools to extract log metadata from source files.
 *
 * See python/src/min_logger/parser_main.py for tools to parse binary log files to extract log
 * messages, values, and profiling.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//////////////////////////////// Configuration Macros ////////////////////////////////

/// Log level constants. Lower values = higher priority.
#define MIN_LOGGER_DEBUG 10     ///< Debug level: detailed diagnostic information
#define MIN_LOGGER_INFO 20      ///< Info level: general informational messages
#define MIN_LOGGER_WARN 30      ///< Warn level: warning messages for recoverable issues
#define MIN_LOGGER_ERROR 40     ///< Error level: error messages for serious problems
#define MIN_LOGGER_CRITICAL 50  ///< Critical level: critical system failures

/// Enable/disable all logging at compile time. Set to 0 to completely disable logging.
#ifndef MIN_LOGGER_ENABLED
    #define MIN_LOGGER_ENABLED 1
#endif

/// Minimum log level to include at compile time. Messages below this level are omitted.
/// This works in conjunction with min_logger_get_level() for dual-stage filtering.
#ifndef MIN_LOGGER_MIN_LEVEL
    #define MIN_LOGGER_MIN_LEVEL (MIN_LOGGER_INFO)
#endif

/// Default runtime log level. Can be changed with min_logger_set_level().
#ifndef MIN_LOGGER_DEFAULT_LEVEL
    #define MIN_LOGGER_DEFAULT_LEVEL MIN_LOGGER_WARN
#endif

//////////////////////////////// Type Definitions ////////////////////////////////

/// Type used for message IDs.
typedef uint32_t MinLoggerCRC;

/**
 * Callback signature for custom log serialization.
 *
 * @param msg_id         Unique identifier for this log message
 * @param payload        Pointer to the data to serialize (NULL if no data)
 * @param payload_len    Length of payload in bytes
 * @param is_fixed_size  True if payload is fixed-size, false if variable-length array.
 *
 * Implementations should serialize these parameters to the desired output medium
 * (serial port, file, network, etc.).
 */
typedef void (*MinLoggerSerializeCallBack)(MinLoggerCRC msg_id, const void* payload,
                                           size_t payload_len, bool is_fixed_size);

#if MIN_LOGGER_ENABLED

////////////////////////////// Helper Macros ////////////////////////////////

    #ifdef __cplusplus
        #include <type_traits>

        #include "min_logger_crc.h"

        /**
         * Generates a compile-time CRC32 ID from a string.
         * Used internally by C++ convenience macros to automatically create message IDs
         * from the source file location (__FILE__:__LINE__).
         */
        #define PRIVATE_MIN_LOGGER_LOG_MSG_GEN_ID(str) \
            static constexpr MinLoggerCRC min_log_id = min_logger_crc::MIN_LOGGER_CPP_CRC32(str);

        /**
         * Provides compile-time type checking in C++.
         * Verifies that a macro argument matches the expected type.
         */
        #define PRIVATE_MIN_LOGGER_ASSERT_TYPE(x, expected_type)                             \
            static_assert(std::is_same<std::decay<decltype(x)>::type, expected_type>::value, \
                          "Type mismatch: expected " #expected_type)

extern "C" {
    #else  // is compiling as C (not C++)
        /**
         * Provides compile-time type checking in C using _Generic.
         * Verifies that a macro argument matches the expected type.
         */
        #define PRIVATE_MIN_LOGGER_ASSERT_TYPE(x, expected_type)        \
            _Static_assert(_Generic((x), expected_type: 1, default: 0), \
                           "Type mismatch: expected " #expected_type)
    #endif

    /// String literal stringification macro
    #define MIN_LOGGER_S1(x) #x
    /// Macro expansion helper
    #define MIN_LOGGER_S2(x) MIN_LOGGER_S1(x)
    /// Generates a unique string based on current file and line number
    #define MIN_LOGGER_LOC __FILE__ ":" MIN_LOGGER_S2(__LINE__)

////////////////////////////// Public API ////////////////////////////////

/**
 * Platform-specific hook: Get current system time in nanoseconds.
 * Weakly linked default implementation uses std::chrono::steady_clock. It can be overriden
 * in platform-specific code (ensure it is extern C).
 *
 * @return Current time in nanoseconds since an arbitrary epoch
 */
uint64_t min_logger_get_time_nanoseconds();

/**
 * Platform-specific hook: Get the current thread's name.
 * Weakly linked default implementation uses pthread_getname_np. It can be overriden
 * in platform-specific code (ensure it is extern C).
 *
 * @param thread_name Pointer to buffer where thread name should be written
 * @param max_len     Maximum length of buffer (including null terminator)
 * @return Number of characters written (not including null terminator), or 0 if unavailable
 */
size_t min_logger_get_thread_name(char* thread_name, size_t max_len);

/**
 * Platform-specific hook: Send serialized message over transport.
 * Weakly linked default implementation uses stdout. It can be overriden
 * in platform-specific code (ensure it is extern C).
 *
 * This is used by the built-in serialization functions.
 *
 * @param msg       Pointer to msg to transmit
 * @param len_bytes Length of msg in bytes
 */
void min_logger_write(const uint8_t* msg, size_t len_bytes);

/**
 * Request all threads to report their names.
 * Thread-safe. Each thread will send its name the next time a log message is emitted
 * after min_logger_write_thread_names() was called.
 */
void min_logger_write_thread_names();

/// Built-in serialization function: Full binary format with timestamps and sync
extern const MinLoggerSerializeCallBack MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT;

/// Built-in serialization function: Minimal binary format for space-constrained systems
extern const MinLoggerSerializeCallBack MIN_LOGGER_MICRO_BINARY_SERIALIZED_FORMAT;

/**
 * Set the serialization format callback.
 * Defaults to MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT if not called.
 *
 * @param serialize_format Function pointer to handle serialization
 */
void min_logger_set_serialize_format(MinLoggerSerializeCallBack serialize_format);

/**
 * Get the current serialization format callback.
 *
 * @return Current serialization callback function pointer
 */
MinLoggerSerializeCallBack min_logger_get_serialize_format();

/**
 * Set the runtime log level filter.
 * Messages below this level will not be serialized at runtime.
 * Works in conjunction with MIN_LOGGER_MIN_LEVEL compile-time filter.
 *
 * @param level MIN_LOGGER_DEBUG, MIN_LOGGER_INFO, etc.
 */
void min_logger_set_level(int level);

/**
 * Get the current runtime log level.
 *
 * @return Current log level threshold
 */
int min_logger_get_level();

/**
 * Send the current thread's name as a variable legnth string if thread name tracking was requested.
 * Called automatically by logging macros when needed.
 *
 * This can be used in custom serialization functions as well.
 */
void send_thread_name_if_needed();

    /**
     * Log a message with an explicit ID.
     *
     * This macro is for C code or when you need explicit control over the message ID.
     * For C++, prefer MIN_LOGGER_LOG() which auto-generates IDs.
     *
     * Compile-time constraints:
     * - id must be a 32-bit unsigned integer literal (not a variable or macro)
     * - level must be an integer or priority constant like MIN_LOGGER_INFO (not a variable)
     * - msg must be a string literal (not a variable). Use ${VALUE_NAME} to reference previously
     *   logged values.
     *
     * Runtime behavior:
     * - Checks MIN_LOGGER_MIN_LEVEL and min_logger_get_level()
     * - Calls the registered serialization callback if both checks pass
     *
     * Example:
     *   MIN_LOGGER_LOG_ID(0xABCD1234, MIN_LOGGER_INFO, "System initialized")
     */
    #define MIN_LOGGER_LOG_ID(id, level, msg)                                   \
        if (MIN_LOGGER_MIN_LEVEL >= level && min_logger_get_level() >= level) { \
            (min_logger_get_serialize_format())(id, NULL, 0, true);             \
        }

    /**
     * Log a single fixed-size value with an explicit ID.
     *
     * Compile-time constraints (extracted by build tools):
     * - id must be a 32-bit unsigned integer literal
     * - level must be an integer or priority constant
     * - name should contain only variable-name-valid characters
     * - type must match the actual type of value. If it is not a primitive type, it must be
     *   described in a type_defs JSON file when generating the mapping. The type must be a plain
     *   old data (POD) type with no pointers or references.
     * - value must be a variable that can be pointed to
     *
     * Runtime behavior:
     * - Serializes the value if level checks pass
     *
     * Example:
     *   float temp = 25.5f;
     *   MIN_LOGGER_RECORD_VALUE_ID(0xABCD1235, MIN_LOGGER_INFO, "temperature", float, temp)
     */
    #define MIN_LOGGER_RECORD_VALUE_ID(id, level, name, type, value)             \
        if (MIN_LOGGER_MIN_LEVEL >= level && min_logger_get_level() >= level) {  \
            PRIVATE_MIN_LOGGER_ASSERT_TYPE(value, type);                         \
            (min_logger_get_serialize_format())(id, &value, sizeof(type), true); \
        }

    /**
     * Identical to MIN_LOGGER_RECORD_VALUE_ID at runtime. The difference is that this macro
     * provides context for tools that extract log metadata to generate a message when this value is
     * sent. In the msg, use ${VALUE_NAME} to reference this or previously logged values.
     */
    #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ID(id, level, name, type, value, msg) \
        MIN_LOGGER_RECORD_VALUE_ID(id, level, name, type, value)

    /**
     * Log a variable-length array with an explicit ID.
     *
     * Compile-time constraints:
     * - id must be a 32-bit unsigned integer literal
     * - level must be a priority constant
     * - name should contain only variable-name-valid characters
     * - type must match the element type of values. If it is not a primitive type, it must be
     *   described in a type_defs JSON file when generating the mapping. The type must be a plain
     *   old data (POD) type with no pointers or references.
     * - values can be a variable but must not contain commas
     * - num_values can be a variable but must not contain commas
     *
     * Runtime behavior:
     * - Serializes all elements: sizeof(type) * num_values bytes
     *
     * Example:
     *   int data[10] = {1, 2, 3, ...};
     *   MIN_LOGGER_RECORD_VALUE_ARRAY_ID(0xABCD1236, MIN_LOGGER_INFO, "sensor_data", int, data, 10)
     */
    #define MIN_LOGGER_RECORD_VALUE_ARRAY_ID(id, level, name, type, values, num_values)        \
        if (MIN_LOGGER_MIN_LEVEL >= level && min_logger_get_level() >= level) {                \
            PRIVATE_MIN_LOGGER_ASSERT_TYPE(*values, type);                                     \
            (min_logger_get_serialize_format())(id, values, sizeof(type) * num_values, false); \
        }

    /**
     * Identical to MIN_LOGGER_RECORD_VALUE_ARRAY_ID at runtime. The difference is that this macro
     * provides context for tools that extract log metadata to generate a message when this value is
     * sent. In the msg, use ${VALUE_NAME} to reference this or previously logged values.
     */
    #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY_ID(id, level, name, type, values, num_values, \
                                                     msg)                                       \
        MIN_LOGGER_RECORD_VALUE_ARRAY_ID(id, level, name, type, values, num_values)

    /**
     * Log entry to a section of code with explicit ID.
     * Used for tracing execution flow.
     *
     * Example:
     *   void my_function() {
     *       MIN_LOGGER_ENTER_ID(0xABCD1237, MIN_LOGGER_DEBUG, my_function)
     *       // ... function body ...
     *   }
     */
    #define MIN_LOGGER_ENTER_ID(id, level, name) MIN_LOGGER_LOG_ID(id, level, name "_enter")

    /**
     * Log function exit with explicit ID.
     * Useful for tracing execution flow.
     *
     * Example:
     *   void my_function() {
     *       MIN_LOGGER_EXIT_ID(0xABCD1238, MIN_LOGGER_DEBUG, my_function)
     *   }
     */
    #define MIN_LOGGER_EXIT_ID(id, level, name) MIN_LOGGER_LOG_ID(id, level, name "_exit")

    // C++ convenience macros that auto-generate the log ID based on source location
    #ifdef __cplusplus
        /**
         * Log a message (C++ only, auto-generates ID).
         *
         * The message ID is automatically generated from __FILE__ and __LINE__,
         * allowing external tools to map IDs back to source locations without
         * requiring explicit ID literals.
         *
         * Compile-time constraints:
         * - level must be an integer or priority constant like MIN_LOGGER_INFO (not a variable)
         * - msg must be a string literal (not a variable). Use ${VALUE_NAME} to reference
         *   previously logged values.
         *
         * Runtime behavior:
         * - Checks MIN_LOGGER_MIN_LEVEL and min_logger_get_level()
         * - Calls the registered serialization callback if both checks pass
         *
         * @param level One of MIN_LOGGER_DEBUG, MIN_LOGGER_INFO, MIN_LOGGER_WARN, MIN_LOGGER_ERROR,
         *              MIN_LOGGER_CRITICAL or an integer literal
         * @param msg   String literal for the log message
         *
         * Example:
         *   MIN_LOGGER_LOG(MIN_LOGGER_INFO, "Application started")
         */
        #define MIN_LOGGER_LOG(level, msg)                         \
            {                                                      \
                PRIVATE_MIN_LOGGER_LOG_MSG_GEN_ID(MIN_LOGGER_LOC); \
                MIN_LOGGER_LOG_ID(min_log_id, level, msg);         \
            }

        /**
         * Log a single fixed-size value (C++ only, auto-generates ID).
         *
         * The message ID is automatically generated from __FILE__ and __LINE__.
         *
         * Compile-time constraints:
         * - level must be an integer or priority constant
         * - name should contain only variable-name-valid characters
         * - type must match the actual type of value. If it is not a primitive type, it must be
         *   described in a type_defs JSON file when generating the mapping. The type must be a
         * plain old data (POD) type with no pointers or references.
         * - value must be a variable that can be pointed to
         *
         * Runtime behavior:
         * - Serializes the value if level checks pass
         *
         * @param level The log level (MIN_LOGGER_DEBUG, MIN_LOGGER_INFO, etc.)
         * @param name  Descriptive name for the value (used in external mapping)
         * @param type  The C++ type of the value (must match actual type)
         * @param value The value to log (type-checked at compile time)
         *
         * Example:
         *   float current_temp = 25.5f;
         *   MIN_LOGGER_RECORD_VALUE(MIN_LOGGER_WARN, "cpu_temp", float, current_temp)
         */
        #define MIN_LOGGER_RECORD_VALUE(level, name, type, value)                 \
            {                                                                     \
                PRIVATE_MIN_LOGGER_LOG_MSG_GEN_ID(MIN_LOGGER_LOC);                \
                MIN_LOGGER_RECORD_VALUE_ID(min_log_id, level, name, type, value); \
            }

        /**
         * Identical to MIN_LOGGER_RECORD_VALUE at runtime. The difference is that this macro
         * provides context for tools that extract log metadata to generate a message when this
         * value is sent. In the msg, use ${VALUE_NAME} to reference this or previously logged
         * values.
         */
        #define MIN_LOGGER_RECORD_AND_LOG_VALUE(level, name, type, value, msg) \
            MIN_LOGGER_RECORD_VALUE(level, name, type, value)

        /**
         * Log a variable-length array (C++ only, auto-generates ID).
         *
         * The message ID is automatically generated from __FILE__ and __LINE__.
         *
         * Compile-time constraints:
         * - level must be an integer or priority constant
         * - name should contain only variable-name-valid characters
         * - type must match the element type of values. If it is not a primitive type, it must be
         *   described in a type_defs JSON file when generating the mapping. The type must be a
         * plain old data (POD) type with no pointers or references.
         * - values can be a variable but must not contain commas
         * - num_values can be a variable but must not contain commas
         *
         * Runtime behavior:
         * - Serializes all elements: sizeof(type) * num_values bytes
         *
         * @param level      The log level
         * @param name       Descriptive name for the array (used in external mapping)
         * @param type       The element type of the array (must be POD)
         * @param values     Pointer to the array
         * @param num_values Number of elements to log
         *
         * Example:
         *   std::vector<int> data = {1, 2, 3, 4, 5};
         *   MIN_LOGGER_RECORD_VALUE_ARRAY(MIN_LOGGER_INFO, "data_points", int, data.data(),
         *                                 data.size())
         */
        #define MIN_LOGGER_RECORD_VALUE_ARRAY(level, name, type, values, num_values)    \
            {                                                                           \
                PRIVATE_MIN_LOGGER_LOG_MSG_GEN_ID(MIN_LOGGER_LOC);                      \
                MIN_LOGGER_RECORD_VALUE_ARRAY_ID(min_log_id, level, name, type, values, \
                                                 num_values);                           \
            }

        /**
         * Identical to MIN_LOGGER_RECORD_VALUE_ARRAY at runtime. The difference is that this macro
         * provides context for tools that extract log metadata to generate a message when this
         * value is sent. In the msg, use ${VALUE_NAME} to reference this or previously logged
         * values.
         */
        #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY(level, name, type, values, num_values, msg) \
            MIN_LOGGER_RECORD_VALUE_ARRAY(level, name, type, values, num_values)

        /**
         * Log entry to a code section (C++ only, auto-generates ID).
         *
         * Used for tracing execution flow. The message ID is automatically generated from
         * __FILE__ and __LINE__.
         *
         * Compile-time constraints:
         * - level must be an integer or priority constant
         * - name should contain only variable-name-valid characters
         *
         * @param level The log level
         * @param name  Name of the function or code section being entered
         *
         * Example:
         *   void process_data() {
         *       MIN_LOGGER_ENTER(MIN_LOGGER_DEBUG, process_data)
         *       // ... function body ...
         *   }
         */
        #define MIN_LOGGER_ENTER(level, name) MIN_LOGGER_LOG(level, name "_enter")

        /**
         * Log exit from a code section (C++ only, auto-generates ID).
         *
         * Used for tracing execution flow. The message ID is automatically generated from
         * __FILE__ and __LINE__.
         *
         * Compile-time constraints:
         * - level must be an integer or priority constant
         * - name should contain only variable-name-valid characters
         *
         * @param level The log level
         * @param name  Name of the function or code section being exited
         *
         * Example:
         *   void process_data() {
         *       MIN_LOGGER_ENTER(MIN_LOGGER_DEBUG, process_data)
         *       // ... function body ...
         *       MIN_LOGGER_EXIT(MIN_LOGGER_DEBUG, process_data)
         *   }
         */
        #define MIN_LOGGER_EXIT(level, name) MIN_LOGGER_LOG(level, name "_exit")

}  // extern "C"
    #endif

#else
// Disabled logging: all functions and macros are no-ops

inline void min_logger_write_thread_names() {}

void min_logger_set_serialize_format(MinLoggerSerializeCallBack serialize_format) {}
MinLoggerSerializeCallBack min_logger_get_serialize_format() { return nullptr; }
void min_logger_set_level(int level) {}
int min_logger_get_level() { return MIN_LOGGER_DEFAULT_LEVEL; }

inline void send_thread_name_if_needed() {}

    #define MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT nullptr
    #define MIN_LOGGER_MICRO_BINARY_SERIALIZED_FORMAT nullptr

    #define MIN_LOGGER_LOG_ID(id, level, msg) \
        do {                                  \
        } while (0)

    #define MIN_LOGGER_RECORD_VALUE_ID(id, level, name, type, value) \
        do {                                                         \
        } while (0)

    #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ID(id, level, name, type, value, msg) \
        do {                                                                      \
        } while (0)

    #define MIN_LOGGER_RECORD_VALUE_ARRAY_ID(id, level, name, type, values, num_values) \
        do {                                                                            \
        } while (0)

    #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY_ID(id, level, name, type, values, num_values, \
                                                     msg)                                       \
        do {                                                                                    \
        } while (0)

    #define MIN_LOGGER_ENTER_ID(level, name, id) \
        do {                                     \
        } while (0)

    #define MIN_LOGGER_EXIT_ID(level, name) \
        do {                                \
        } while (0)

    #ifdef __cplusplus
        #define MIN_LOGGER_LOG(level, msg) \
            do {                           \
            } while (0)

        #define MIN_LOGGER_RECORD_VALUE(level, name, type, value) \
            do {                                                  \
            } while (0)

        #define MIN_LOGGER_RECORD_AND_LOG_VALUE(level, name, type, value, msg) \
            do {                                                               \
            } while (0)

        #define MIN_LOGGER_RECORD_VALUE_ARRAY(level, name, type, values, num_values) \
            do {                                                                     \
            } while (0)

        #define MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY(level, name, type, values, num_values) \
            do {                                                                             \
            } while (0)

        #define MIN_LOGGER_ENTER(level, name) \
            do {                              \
            } while (0)

        #define MIN_LOGGER_EXIT(level, name) \
            do {                             \
            } while (0)
    #endif
#endif
