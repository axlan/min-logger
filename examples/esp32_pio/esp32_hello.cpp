#include <Arduino.h>

#include <min_logger.h>

extern "C" {

size_t min_logger_get_thread_name(char* thread_name, size_t max_len){
    return 0;
}

uint64_t get_time_nanoseconds() {
    return micros() * 1000;
}

}

void min_logger_write(const uint8_t* msg, size_t len_bytes) { Serial.write(msg, len_bytes); }

void setup(){
    Serial.begin(115200);
}

void loop() {
    MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world");
    delay(2000);
}
