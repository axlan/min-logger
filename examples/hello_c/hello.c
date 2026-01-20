#include <min_logger/min_logger.h>
#include <stdio.h>

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

void min_logger_write(const uint8_t* msg, size_t len_bytes) {
    print_bytes_as_hex_columns(msg, len_bytes, 4);
}

int main() {
    min_logger_write_thread_names();
    MIN_LOGGER_LOG_ID(0, MIN_LOGGER_INFO, "hello world binary");
}
