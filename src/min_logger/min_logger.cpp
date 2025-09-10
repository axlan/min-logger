// Don't expect _min_logger_gen.h to exist.
#define MIN_LOGGER_LOG_INTERNAL_LIB true
#include "min_logger.h"

#include <cstdio>

static constexpr const char* SEVERITY_STRING(int severity) {
    return (severity <= MIN_LOGGER_DEBUG)   ? "DEBUG"
           : (severity <= MIN_LOGGER_INFO)  ? "INFO"
           : (severity <= MIN_LOGGER_WARN)  ? "WARN"
           : (severity <= MIN_LOGGER_ERROR) ? "ERROR"
                                            : "CRITICAL";
}

extern "C" {

const char** MIN_LOGGER_NO_TAGS = {nullptr};

void __attribute__((weak)) min_logger_write(const char* msg) { puts(msg); }

void __attribute__((weak)) min_logger_format_and_write(const char** tags, const char* file_name,
                                                       unsigned int line_number, const char* msg,
                                                       int severity, time_point_value_t timestamp) {
    static constexpr size_t _MAX_MSG_SIZE = 1024;
    char msg_buffer[_MAX_MSG_SIZE] = {0};

    float time_sec = timestamp / 1e9;

    snprintf(msg_buffer, _MAX_MSG_SIZE, "%.3f %s %s:%u] %s\n", time_sec, SEVERITY_STRING(severity),
             file_name, line_number, msg);
    min_logger_write(msg_buffer);
}
}
