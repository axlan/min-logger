#include <min_logger/min_logger.h>

int main() {
    // *min_logging_is_verbose() = true;
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world", 0);
}
