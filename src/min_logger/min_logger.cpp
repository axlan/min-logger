
#include <cstdio>

extern "C" {

void __attribute__((weak)) min_logger_write(const char* msg) { puts(msg); }
}
