#include <cstdio>

#include <min_logger/min_logger.h>

static void print_bytes_as_hex_columns(const uint8_t* data, size_t len, int column_size) {
    if (data == NULL || len == 0 || column_size <= 0) {
        return;
    }

    for (size_t i = 0; i < len; ++i) {
        printf("%02X ", data[i]);  // Print each byte as 2-digit hex

        // Check if it's time to start a new line (after 'columns' bytes)
        if ((i + 1) % column_size == 0) {
            printf("\n");
        }
    }

    // If the last line wasn't fully filled, add a newline at the end
    if (len % column_size != 0) {
        printf("\n");
    }
}

extern "C" {

void min_logger_write(const uint8_t* msg, size_t len_bytes) {
    if (*min_logger_serialize_format() == MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT) {
        print_bytes_as_hex_columns(msg, len_bytes, 4);
    } else {
        fwrite(msg, sizeof(uint8_t), len_bytes, stdout);
    }
}

}

int main() {
    printf("Truncated Logging:\n");
    min_logger_write_thread_names();
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world trunc");
    MIN_LOGGER_LOG_ID(0xDEADBEEF, MIN_LOGGER_INFO, "hello world trunc, explicit ID");

    printf("\nVerbose Logging:\n");
    *min_logger_is_verbose() = true;
    min_logger_write_thread_names();
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world verbose");

    printf("\nBinary Logging:\n");
    *min_logger_is_verbose() = false;
    *min_logger_serialize_format() = MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT;
    min_logger_write_thread_names();
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world binary");
}
