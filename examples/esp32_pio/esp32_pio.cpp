#include <Arduino.h>
#include <min_logger.h>

#include "driver/uart.h"

void setup() {
    min_logger_set_serialize_format(MIN_LOGGER_MICRO_BINARY_SERIALIZED_FORMAT);
    Serial.begin(115200);
}

#ifdef MIN_LOGGER_BUFFERED_ESP32_PLATFORM

void loop() {
    static bool switched_to_serial = false;
    auto start_time = micros();

    for (int i = 0; i < MIN_LOGGER_BUFFER_SIZE / 4; i++) {
        MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world");
    }

    auto elapsed = micros() - start_time;

    Serial.printf("{%02x %02x %02x %02x}\n", min_logger_buffer[0], min_logger_buffer[1],
                  min_logger_buffer[2], min_logger_buffer[3]);

    Serial.printf("%d log calls took: %luus\n", MIN_LOGGER_BUFFER_SIZE, elapsed);

    delay(10000);

    if (!switched_to_serial) {
        switched_to_serial = true;
        min_logger_init_uart(UART_NUM_0);
    }
}

#else




void loop() {
    static constexpr size_t NUM_WRITES = 256;
    auto start_time = micros();

    for (int i = 0; i < NUM_WRITES / 4; i++) {
        MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello world");
    }

    auto elapsed = micros() - start_time;
    Serial.printf("%d log calls took: %luus\n", NUM_WRITES, elapsed);

    delay(10000);
}

#endif