#include <min_logger/min_logger.h>

int main() {
    min_logger_write_thread_names();
    // Example parsed log message:
    // 15328834.308338 INFO  examples/hello_c/hello.c:7 hello_c] hello world binary
    MIN_LOGGER_LOG_ID(0, MIN_LOGGER_INFO, "hello world binary");
}
