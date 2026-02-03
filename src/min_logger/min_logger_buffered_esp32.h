/*
 * Buffered ESP32 Platform Implementation
 *
 * Provides lock-free ring buffer-based logging for ESP32 with support for
 * UART and UDP output. Logging writes are non-blocking, with separate tasks
 * handling output to avoid blocking the logger.
 *
 * This overrides the:
 * void __attribute__((weak)) IRAM_ATTR min_logger_write(const uint8_t* msg, size_t len_bytes)
 * 
 * MIN_LOGGER_BUFFERED_ESP32_PLATFORM must be defined to use this over minimal implemetation in
 * src/min_logger/platform_implementations/defaults.cpp
 */

#pragma once

#include "min_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MIN_LOGGER_ENABLED && defined(MIN_LOGGER_BUFFERED_ESP32_PLATFORM)

    // Buffer size for the lock-free ring buffer (must be power of two)
    #ifndef MIN_LOGGER_BUFFER_SIZE
        #define MIN_LOGGER_BUFFER_SIZE 256
    #endif

    // Compile in UDP logging functionality
    #ifndef MIN_LOGGER_ENABLE_UDP
        #define MIN_LOGGER_ENABLE_UDP 1
    #endif

// Global buffer for logging data (used for post mortem or core dump)
extern uint8_t min_logger_buffer[MIN_LOGGER_BUFFER_SIZE];

// Initialize UART output for min logger
// Starts a min_logger_uart task
// \param uart_num UART port number to use for logging
void min_logger_init_uart(unsigned uart_num);

    #if MIN_LOGGER_ENABLE_UDP
// Initialize UDP output for min logger
// Starts a min_logger_udp task
// Logs will only start being sent when the network is up. Logs from before are ignored.
//
// \param packet_size Size of each UDP packet to send. Messages wait in buffer
//                    until this size is reached. MIN_LOGGER_BUFFER_SIZE must
//                    be integer multiple of packet_size.
// \param poll_interval_ms Polling interval in milliseconds for the UDP task
// \param logging_udp_ip Destination IP address for UDP packets
// \param logging_udp_port Destination port for UDP packets
void min_logger_init_udp(size_t packet_size, unsigned poll_interval_ms, const char* logging_udp_ip,
                         uint16_t logging_udp_port);
    #endif

#else
inline void min_logger_init_uart(unsigned uart_num) {}
inline void min_logger_init_udp(size_t packet_size, unsigned poll_interval_ms,
                                const char* logging_udp_ip, uint16_t logging_udp_port) {}
#endif

#ifdef __cplusplus
}
#endif
