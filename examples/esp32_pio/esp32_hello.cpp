#include <Arduino.h>

#include <min_logger.h>

void min_logger_get_thread_name(char* thread_name) {
    strcpy(thread_name, "main");
}

uint64_t get_time_nanoseconds() {
    return micros() * 1000;
}

void min_logger_write(const uint8_t* msg, size_t len_bytes) { Serial.write(msg, len_bytes); }

void setup(){
    Serial.begin(115200);
}

void loop() {
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world", 0);
    delay(2000);
}
