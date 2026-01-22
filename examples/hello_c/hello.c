#include <min_logger/min_logger.h>

int main() {
    min_logger_write_thread_names();
    MIN_LOGGER_LOG_ID(0, MIN_LOGGER_INFO, "hello world binary");
}
