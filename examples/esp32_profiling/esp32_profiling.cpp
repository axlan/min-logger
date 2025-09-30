#include <Arduino.h>
#include "driver/uart.h"

#include <min_logger.h>

static constexpr size_t MAX_BUFFER_LEN = 4;
static uint8_t buffer[MAX_BUFFER_LEN] = {0};

extern "C" {

size_t min_logger_get_thread_name(char* thread_name, size_t max_len) { return 0; }

uint64_t get_time_nanoseconds() { return micros() * 1000; }

// Profile log call without actual write.
void min_logger_write(const uint8_t* msg, size_t len_bytes) { memcpy(buffer, msg, MAX_BUFFER_LEN); }
}

void setup() { Serial.begin(115200); }

void loop() {

    *min_logger_serialize_format() = MIN_LOGGER_MICRO_BINARY_SERIALIZED_FORMAT;

    auto start_time = micros();

    for(int i =0; i < 1000; i++) {
        MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world");
    }

    auto elapsed = micros() - start_time;

    Serial.printf("{%02x %02x %02x %02x}\n", buffer[0], buffer[1], buffer[2], buffer[3]);

    // 2.93-3.01us per call
    Serial.printf("1000 log calls took: %luus\n", elapsed);

    Serial.printf("UART_HW_FIFO_LEN: %u\n", UART_FIFO_LEN);

    delay(1000);

    start_time = micros();
    Serial.print('\n');
    elapsed = micros() - start_time;

    // 17-37us per call
    Serial.printf("1 char took: %luus\n", elapsed);

    delay(1000);

    char dummy_buf[201];
    memset(dummy_buf, 'a', 100);
    dummy_buf[100] = 0;

    start_time = micros();
    Serial.print(dummy_buf);
    elapsed = micros() - start_time;

    // 26-35us per call
    Serial.printf("\n100 char took: %luus\n", elapsed);

    delay(1000);

    memset(dummy_buf, 'a', 200);
    dummy_buf[200] = 0;

    start_time = micros();
    Serial.print(dummy_buf);
    elapsed = micros() - start_time;

    // 10152-10167us per call
    Serial.printf("\n200 char took: %luus\n", elapsed);

    delay(10000);
}
