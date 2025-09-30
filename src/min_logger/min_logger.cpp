#include "min_logger.h"

#if MIN_LOGGER_ENABLED
    #include <algorithm>
    #include <atomic>
    #include <chrono>
    #include <cstdio>
    #include <cstring>
    #include <thread>

static constexpr uint32_t THREAD_NAME_MSG_ID = 0XFFFFFF00;
static constexpr size_t PTHREAD_NAME_LEN = 16;

static std::atomic<int> thread_count = {0};

static std::atomic<unsigned> name_broadcast_count = {0};

thread_local int local_thread_idx = -1;
thread_local unsigned local_name_broadcast_count = 0;

void min_logger_write_thread_names() { name_broadcast_count++; }

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

void send_thread_name_if_needed() {
    // Handles overflow implicitly
    if (local_name_broadcast_count != name_broadcast_count) {
        local_name_broadcast_count = name_broadcast_count;
        char name_buffer[PTHREAD_NAME_LEN] = {0};
        size_t name_len = min_logger_get_thread_name(name_buffer, PTHREAD_NAME_LEN);
        (*min_logger_serialize_format())(THREAD_NAME_MSG_ID, MIN_LOGGER_PAYLOAD_STRING, name_buffer,
                                         name_len);
    }
}

static size_t get_thread_idx() {
    if (local_thread_idx == -1) {
        local_thread_idx = thread_count++;
    }
    return local_thread_idx;
}

static void payload_to_string_end(char* buffer, size_t buffer_size, PayloadType payload_type,
                                  const void* payload, size_t payload_len) {
    if (buffer_size < 2) {
        return;
    }

    int snprintf_size = 0;

    if (payload_len > 0 && payload_type != MIN_LOGGER_PAYLOAD_NONE) {
        // Leave space for '\n'
        const int payload_buffer_size = buffer_size - 1;

        switch (payload_type) {
            case MIN_LOGGER_PAYLOAD_STRING:
                snprintf_size =
                    snprintf(buffer, payload_buffer_size, "%s", static_cast<const char*>(payload));
                break;
            case MIN_LOGGER_PAYLOAD_U64:
                snprintf_size = snprintf(buffer, payload_buffer_size, "%" PRIu64,
                                         *static_cast<const u_int64_t*>(payload));
                break;
            case MIN_LOGGER_PAYLOAD_NONE:
                break;
        }

        // Don't count the newline since it will be overridden.
        if (snprintf_size > payload_buffer_size - 1) {
            snprintf_size = payload_buffer_size - 1;
        }
    }

    buffer[snprintf_size] = '\n';
    buffer[snprintf_size + 1] = 0;
}

extern "C" {

size_t __attribute__((weak)) min_logger_get_thread_name(char* thread_name, size_t max_len) {
    auto thread = pthread_self();
    if (pthread_getname_np(thread, thread_name, max_len) == 0) {
        return strlen(thread_name);
    }
    return 0;
}

uint64_t __attribute__((weak)) get_time_nanoseconds() {
    auto time_since_epoch = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(time_since_epoch).count();
}

void __attribute__((weak)) min_logger_write(const uint8_t* msg, size_t len_bytes) {
    fwrite(msg, sizeof(uint8_t), len_bytes, stdout);
}

    #pragma pack(1)  // Set packing alignment to 1 byte
struct BinaryMsgHeader {
    static constexpr uint16_t SYNC = 0xFAAF;
    uint16_t sync = SYNC;
    uint8_t payload_len = 0;
    uint8_t thread_id = 0;
    MinLoggerCRC msg_id = 0;
    uint64_t timestamp = 0;
};
    #pragma pack()  // Revert to default packing alignment

void min_logger_default_binary_serializer(MinLoggerCRC msg_id, PayloadType payload_type,
                                          const void* payload, size_t payload_len) {
    static constexpr size_t _MAX_MSG_SIZE = 256;
    static constexpr size_t _MAX_PAYLOAD_SIZE = _MAX_MSG_SIZE - sizeof(BinaryMsgHeader);
    payload_len = (payload_len > _MAX_PAYLOAD_SIZE) ? _MAX_PAYLOAD_SIZE : payload_len;

    uint8_t msg_buffer[_MAX_MSG_SIZE];
    BinaryMsgHeader* header_ptr = reinterpret_cast<BinaryMsgHeader*>(msg_buffer);
    header_ptr->sync = BinaryMsgHeader::SYNC;
    header_ptr->msg_id = msg_id;
    header_ptr->payload_len = payload_len;
    header_ptr->timestamp = get_time_nanoseconds();
    header_ptr->thread_id = get_thread_idx();
    memcpy(msg_buffer + sizeof(BinaryMsgHeader), payload, payload_len);
    min_logger_write(msg_buffer, sizeof(BinaryMsgHeader) + payload_len);
}
const MinLoggerSerializeCallBack MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT =
    min_logger_default_binary_serializer;

void min_logger_default_text_serializer(MinLoggerCRC msg_id, PayloadType payload_type,
                                        const void* payload, size_t payload_len) {
    static constexpr size_t _MAX_MSG_SIZE = 256;
    char msg_buffer[_MAX_MSG_SIZE] = {0};
    uint64_t nanoseconds = get_time_nanoseconds();
    double time_sec = nano_to_seconds(nanoseconds);
    size_t thread_id = get_thread_idx();

    size_t offset =
        snprintf(msg_buffer, _MAX_MSG_SIZE, "$%.6f,%08X,%zu", time_sec, msg_id, thread_id);

    if (payload_len > 0 && payload_type != MIN_LOGGER_PAYLOAD_NONE) {
        msg_buffer[offset] = ',';
        offset++;
    }
    payload_to_string_end(msg_buffer + offset, _MAX_MSG_SIZE - offset, payload_type, payload,
                          payload_len);

    min_logger_write(reinterpret_cast<uint8_t*>(msg_buffer), strlen(msg_buffer));
}
const MinLoggerSerializeCallBack MIN_LOGGER_DEFAULT_TEXT_SERIALIZED_FORMAT =
    min_logger_default_text_serializer;

void min_logger_default_verbose_format(MinLoggerCRC msg_id, const char* file_name,
                                       unsigned int line_number, const char* function_name,
                                       const char* msg, int severity, PayloadType payload_type,
                                       const void* payload, size_t payload_len) {
    #if !MIN_LOGGER_DISABLE_VERBOSE_LOGGING
    static constexpr size_t _MAX_MSG_SIZE = 1024;
    char msg_buffer[_MAX_MSG_SIZE] = {0};

    uint64_t nanoseconds = get_time_nanoseconds();
    double time_sec = nano_to_seconds(nanoseconds);
    char name_buffer[PTHREAD_NAME_LEN] = {0};
    min_logger_get_thread_name(name_buffer, PTHREAD_NAME_LEN);

    int offset = snprintf(msg_buffer, _MAX_MSG_SIZE, "%.3f %s %s:%u %s] %s", time_sec,
                          SEVERITY_STRING(severity), file_name, line_number, name_buffer, msg);

    if (payload_len > 0 && payload_type != MIN_LOGGER_PAYLOAD_NONE) {
        msg_buffer[offset] = ':';
        msg_buffer[offset + 1] = ' ';
        offset += 2;
    }
    payload_to_string_end(msg_buffer + offset, _MAX_MSG_SIZE - offset, payload_type, payload,
                          payload_len);

    min_logger_write(reinterpret_cast<uint8_t*>(msg_buffer), strlen(msg_buffer));
    #endif
}
const MinLoggerVerboseCallBack MIN_LOGGER_DEFAULT_VERBOSE_FORMAT =
    min_logger_default_verbose_format;

bool* min_logger_is_verbose() {
    static bool is_verbose = MIN_LOGGER_DEFAULT_VERBOSE;
    return &is_verbose;
}

MinLoggerVerboseCallBack* min_logger_verbose_format() {
    static MinLoggerVerboseCallBack verbose_format = MIN_LOGGER_DEFAULT_VERBOSE_FORMAT;
    return &verbose_format;
}

MinLoggerSerializeCallBack* min_logger_serialize_format() {
    static MinLoggerSerializeCallBack serialize_format = MIN_LOGGER_DEFAULT_TEXT_SERIALIZED_FORMAT;
    return &serialize_format;
}

int* min_logger_level() {
    static int level = MIN_LOGGER_DEFAULT_LEVEL;
    return &level;
}

}  // extern "C"

#endif  // MIN_LOGGER_ENABLED
