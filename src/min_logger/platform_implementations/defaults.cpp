#include "../min_logger.h"

#if MIN_LOGGER_ENABLED
    #if defined(ESP32) || defined(ESP_PLATFORM)
        #include <driver/uart.h>
        #include <esp_timer.h>
        #include <freertos/FreeRTOS.h>
        #include <freertos/task.h>
    #else
        #include <chrono>
        #include <cstdio>
        #include <thread>
    #endif

    #ifdef __cplusplus
extern "C" {
    #endif

    #if defined(ESP32) || defined(ESP_PLATFORM)

size_t __attribute__((weak)) min_logger_get_thread_name(char* thread_name, size_t max_len) {
    char* taskName = pcTaskGetName(NULL);
    strncpy(thread_name, taskName, max_len);
    thread_name[max_len - 1] = 0;
    return strlen(thread_name);
}

uint64_t __attribute__((weak)) IRAM_ATTR min_logger_get_time_nanoseconds() {
    return esp_timer_get_time() * 1000;
}

void __attribute__((weak)) IRAM_ATTR min_logger_write(const uint8_t* msg, size_t len_bytes) {
    uart_write_bytes(UART_NUM_0, msg, len_bytes);
}

    ////////////////////////////////////// Posix //////////////////////////////////////////////////
    #else

size_t __attribute__((weak)) min_logger_get_thread_name(char* thread_name, size_t max_len) {
    auto thread = pthread_self();
    if (pthread_getname_np(thread, thread_name, max_len) == 0) {
        return strlen(thread_name);
    }
    return 0;
}

uint64_t __attribute__((weak)) min_logger_get_time_nanoseconds() {
    auto time_since_epoch = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time_since_epoch).count();
}

void __attribute__((weak)) min_logger_write(const uint8_t* msg, size_t len_bytes) {
    fwrite(msg, sizeof(uint8_t), len_bytes, stdout);
}
    #endif

    #ifdef __cplusplus
}
    #endif

#endif