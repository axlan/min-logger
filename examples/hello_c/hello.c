#include <min_logger/min_logger.h>
#include <hex_printer.h>

void min_logger_write(const uint8_t* msg, size_t len_bytes) {
    print_bytes_as_hex_columns(msg, len_bytes, 4);
}

int main() {
    min_logger_write_thread_names();
    MIN_LOGGER_LOG_ID(0, MIN_LOGGER_INFO, "hello world binary");
}
