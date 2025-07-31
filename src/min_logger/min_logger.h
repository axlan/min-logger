#pragma once

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

#if MIN_LOGGER_ENABLED

extern void min_logger_write(const char* msg);

    #include "_min_logger_gen.h" // NOLINT

    #define MIN_LOGGER_LOG(level, msg, id) min_logger_func_##id()

#else
    #error "NEED TO IMPLEMENT!"
#endif

#ifdef __cplusplus
}
#endif
