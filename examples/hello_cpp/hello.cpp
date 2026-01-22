#include <min_logger/min_logger.h>

int main() {
    min_logger_write_thread_names();
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world binary");
}
