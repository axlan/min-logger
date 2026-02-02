#pragma once

#include "min_logger.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MIN_LOGGER_ENABLED

    // For platforms that buffer log data, the size of the buffer in bytes.
    #ifndef MIN_LOGGER_BUFFER_SIZE
        #define MIN_LOGGER_BUFFER_SIZE 256
    #endif

    #ifndef MIN_LOGGER_ENABLE_UDP
        #define MIN_LOGGER_ENABLE_UDP 1
    #endif

extern uint8_t min_logger_buffer[MIN_LOGGER_BUFFER_SIZE];

void min_logger_init_uart(unsigned uart_num);

    #if MIN_LOGGER_ENABLE_UDP
void min_logger_init_udp(size_t packet_size, unsigned poll_interval_ms, const char* logging_udp_ip,
                         uint16_t logging_udp_port);
    #endif

#else

#endif

#ifdef __cplusplus
}
#endif
