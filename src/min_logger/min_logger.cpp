// Don't expect _min_logger_gen.h to exist.
#define MIN_LOGGER_LOG_INTERNAL_LIB true
#include "min_logger.h"

#if MIN_LOGGER_ENABLED
    #include <atomic>
    #include <chrono>
    #include <cstdio>
    #include <cstring>
    #include <thread>

bool* min_logging_is_verbose() {
    static bool is_verbose = MIN_LOGGER_VERBOSE_LOGGING_BY_DEFAULT;
    return &is_verbose;
}

    #if !MIN_LOGGER_DISABLE_VERBOSE_LOGGING

static constexpr const char* SEVERITY_STRING(int severity) {
    return (severity <= MIN_LOGGER_DEBUG)   ? "DEBUG"
           : (severity <= MIN_LOGGER_INFO)  ? "INFO"
           : (severity <= MIN_LOGGER_WARN)  ? "WARN"
           : (severity <= MIN_LOGGER_ERROR) ? "ERROR"
                                            : "CRITICAL";
}

    #endif

static double nano_to_seconds(uint64_t nanoseconds) {
    static constexpr uint64_t TO_NANO = 1000000000ull;
    uint64_t time_sec = nanoseconds / TO_NANO;
    return double(time_sec) + double(nanoseconds - time_sec * TO_NANO) / double(TO_NANO);
}

static std::atomic<int> thread_count = {0};

static std::atomic<int> name_broadcast_count = {0};

thread_local int local_thread_idx = -1;
thread_local int local_name_broadcast_count = 0;

static size_t get_thread_idx() {
    if (local_thread_idx == -1) {
        local_thread_idx = thread_count++;
    }
    return local_thread_idx;
}

extern "C" {

const char** MIN_LOGGER_NO_TAGS = {nullptr};
static constexpr size_t PTHREAD_NAME_LEN = 16;

void __attribute__((weak)) min_logger_get_thread_name(char* thread_name) {
    auto thread = pthread_self();
    pthread_getname_np(thread, thread_name, PTHREAD_NAME_LEN);
}

uint64_t __attribute__((weak)) get_time_nanoseconds() {
    auto time_since_epoch = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time_since_epoch).count();
}

void min_logger_write_thread_names() { name_broadcast_count++; }

void __attribute__((weak)) min_logger_write(const char* msg) { fputs(msg, stdout); }

void __attribute__((weak)) min_logger_write_msg_from_id(const char* log_msg_id, const char* payload,
                                                        size_t payload_len) {
    static constexpr size_t _MAX_MSG_SIZE = 64;
    char msg_buffer[_MAX_MSG_SIZE] = {0};
    uint64_t nanoseconds = get_time_nanoseconds();
    double time_sec = nano_to_seconds(nanoseconds);
    size_t thread_id = get_thread_idx();
    if (payload_len > 0) {
        snprintf(msg_buffer, _MAX_MSG_SIZE, "$%.6f,%s,%zu,%s\n", time_sec, log_msg_id, thread_id,
                 payload);
    } else {
        snprintf(msg_buffer, _MAX_MSG_SIZE, "$%.6f,%s,%zu\n", time_sec, log_msg_id, thread_id);
    }

    min_logger_write(msg_buffer);

    if (local_name_broadcast_count < name_broadcast_count) {
        local_name_broadcast_count++;
        char name_buffer[PTHREAD_NAME_LEN] = {0};
        min_logger_get_thread_name(name_buffer);
        snprintf(msg_buffer, _MAX_MSG_SIZE, "$%.6f,TID,%zu,%s\n", time_sec, thread_id, name_buffer);
        min_logger_write(msg_buffer);
    }
}

void __attribute__((weak)) min_logger_format_and_write_log(const char** tags, const char* file_name,
                                                           unsigned int line_number,
                                                           const char* msg, int severity) {
    #if !MIN_LOGGER_DISABLE_VERBOSE_LOGGING
    static constexpr size_t _MAX_MSG_SIZE = 1024;
    char msg_buffer[_MAX_MSG_SIZE] = {0};

    uint64_t nanoseconds = get_time_nanoseconds();
    double time_sec = nano_to_seconds(nanoseconds);

    snprintf(msg_buffer, _MAX_MSG_SIZE, "%.3f %s %s:%u] %s\n", time_sec, SEVERITY_STRING(severity),
             file_name, line_number, msg);
    min_logger_write(msg_buffer);
    #endif
}
}

#endif  // MIN_LOGGER_ENABLED
