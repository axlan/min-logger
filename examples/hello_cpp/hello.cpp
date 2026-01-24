#include <min_logger/min_logger.h>

int main() {
    min_logger_write_thread_names();
    // Example parsed log message:
    // 15328834.560464 INFO  examples/hello_cpp/hello.cpp:7 hello_cpp] hello world binary
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world binary");
}
