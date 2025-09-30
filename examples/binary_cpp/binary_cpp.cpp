#include <cstdio>

#include <min_logger/min_logger.h>


int main() {
    *min_logger_is_verbose() = false;
    *min_logger_serialize_format() = MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT;
    min_logger_write_thread_names();
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world binary");
}
