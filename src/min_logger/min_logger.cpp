#include "min_logger.h"

#if MIN_LOGGER_ENABLED
    #include <algorithm>
    #include <atomic>
    #include <chrono>
    #include <cmath>
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

// Converts elapsed time in nanoseconds to (scale, value) pair
// Scale: 0=ns, 1=us, 2=ms, 3=s
// Value: 0-999
static std::pair<unsigned, unsigned> convertNanoseconds(uint64_t ns) {
    unsigned scale = 0;
    uint64_t value = ns;

    // Scale up until value fits in 0-999 range
    if (value >= 1000) {
        value /= 1000;
        scale = 1;  // microseconds

        if (value >= 1000) {
            value /= 1000;
            scale = 2;  // milliseconds

            if (value >= 1000) {
                value /= 1000;
                scale = 3;  // seconds

                // Cap at 999 seconds
                if (value > 999) {
                    value = 999;
                }
            }
        }
    }

    return {scale, static_cast<unsigned>(value)};
}

static size_t get_thread_idx() {
    if (local_thread_idx == -1) {
        local_thread_idx = thread_count++;
    }
    return local_thread_idx;
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

void send_thread_name_if_needed() {
    // Handles overflow implicitly
    if (local_name_broadcast_count != name_broadcast_count) {
        local_name_broadcast_count = name_broadcast_count;
        char name_buffer[PTHREAD_NAME_LEN] = {0};
        size_t name_len = min_logger_get_thread_name(name_buffer, PTHREAD_NAME_LEN);
        (*min_logger_serialize_format())(THREAD_NAME_MSG_ID, name_buffer, name_len, false);
    }
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

void min_logger_default_binary_serializer(MinLoggerCRC msg_id, const void* payload,
                                          size_t payload_len, bool is_fixed_size) {
    send_thread_name_if_needed();

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

    #pragma pack(1)  // Set packing alignment to 1 byte
struct MicroMessageHeader {
    uint16_t truncated_id;
    uint8_t thread_id : 4;     // 4 bits
    uint8_t time_scale : 2;    // 2 bits
    uint16_t time_value : 10;  // 10 bits

    MicroMessageHeader() : truncated_id(0), thread_id(0), time_scale(0), time_value(0) {}

    MicroMessageHeader(MinLoggerCRC id, uint8_t thread, uint8_t scale, uint16_t value)
        : truncated_id(static_cast<uint16_t>(id)),
          thread_id(thread & 0xF),
          time_scale(scale & 0x3),
          time_value(value & 0x3FF) {}
};
    #pragma pack()

void min_logger_micro_binary_serializer(MinLoggerCRC msg_id, const void* payload,
                                        size_t payload_len, bool is_fixed_size) {
    send_thread_name_if_needed();
    static std::atomic<uint64_t> last_timestamp_ns = {0};

    uint64_t current_timestamp_ns = get_time_nanoseconds();
    uint64_t local_last_timestamp_ns = last_timestamp_ns.exchange(current_timestamp_ns);
    uint64_t elapsed_ns = 0;
    // Handle initial case, and race condition between computing current time and doing exchange.
    // There still may be an issue here where the delta can be sent out of order.
    if (local_last_timestamp_ns != 0 && current_timestamp_ns > local_last_timestamp_ns) {
        elapsed_ns = current_timestamp_ns - local_last_timestamp_ns;
    }

    auto delta = convertNanoseconds(elapsed_ns);

    static constexpr size_t _MAX_MSG_SIZE = 256;
    static constexpr size_t _MAX_PAYLOAD_SIZE = _MAX_MSG_SIZE - sizeof(BinaryMsgHeader);
    payload_len = (payload_len > _MAX_PAYLOAD_SIZE) ? _MAX_PAYLOAD_SIZE : payload_len;
    size_t total_size = payload_len + sizeof(MicroMessageHeader);

    uint8_t msg_buffer[_MAX_MSG_SIZE];
    MicroMessageHeader* header_ptr = reinterpret_cast<MicroMessageHeader*>(msg_buffer);
    *header_ptr = MicroMessageHeader(msg_id, get_thread_idx(), delta.first, delta.second);

    if (payload_len > 0) {
        uint8_t* payload_ptr = msg_buffer + sizeof(MicroMessageHeader);
        if (!is_fixed_size) {
            *payload_ptr = payload_len;
            payload_ptr++;
            total_size++;
        }
        memcpy(payload_ptr, payload, payload_len);
    }

    min_logger_write(msg_buffer, total_size);
}
const MinLoggerSerializeCallBack MIN_LOGGER_MICRO_BINARY_SERIALIZED_FORMAT =
    min_logger_micro_binary_serializer;

MinLoggerSerializeCallBack* min_logger_serialize_format() {
    static MinLoggerSerializeCallBack serialize_format =
        MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT;
    return &serialize_format;
}

int* min_logger_level() {
    static int level = MIN_LOGGER_DEFAULT_LEVEL;
    return &level;
}

}  // extern "C"

#endif  // MIN_LOGGER_ENABLED
