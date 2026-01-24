#include <min_logger/min_logger.h>

#pragma pack(1)  // Set packing alignment to 1 byte

struct Point {
    float x;
    float y;
    int vals[2];
};

struct Rect {
    Point pos;
    Point size;
    uint8_t padding[10];
    char str[10];
    uint8_t bytes[10];
};

#pragma pack()  // Revert to default packing alignment

int main() {
    min_logger_write_thread_names();

    int a = 100;
    // Example parsed log message:
    // 15328834.815283 INFO  examples/custom_type/custom_type.cpp:27 custom_type] An integer value: 100
    MIN_LOGGER_RECORD_AND_LOG_VALUE(MIN_LOGGER_INFO, "test_int", int, a,
                                    "An integer value: ${test_int}");

    char msg[] = "hello";
    // Example parsed log message:
    // 15328834.815283 INFO  examples/custom_type/custom_type.cpp:33 custom_type] An integer value: hello
    MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY(MIN_LOGGER_INFO, "test_str", char, msg, strlen(msg),
                                          "An integer value: ${test_str}");

    Rect rects[2] = {{Point{0.0f, 0.0f, {1, 2}}, Point{5.0f, 5.0f}, {1}, "ccat", {2}},
                     {Point{1.0f, 1.0f}, Point{10.0f, 10.0f}}};
    // Example parsed log message:
    // 15328834.815285 INFO  examples/custom_type/custom_type.cpp:34 custom_type] rectangle: {'pos': {'x': 0.0, 'y': 0.0, 'dummy': [1, 2]}, 'size': {'x': 5.0, 'y': 5.0, 'dummy': [0, 0]}, 'str': 'ccat', 'bytes': b'\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00'}
    MIN_LOGGER_RECORD_AND_LOG_VALUE(MIN_LOGGER_INFO, "test_rect", Rect, rects[0],
                                    "rectangle: ${test_rect}");
    // Example parsed log message:
    // 15328834.815285 INFO  examples/custom_type/custom_type.cpp:36 custom_type] rectangle: [{'pos': {'x': 0.0, 'y': 0.0, 'dummy': [1, 2]}, 'size': {'x': 5.0, 'y': 5.0, 'dummy': [0, 0]}, 'str': 'ccat', 'bytes': b'\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00'}, {'pos': {'x': 1.0, 'y': 1.0, 'dummy': [0, 0]}, 'size': {'x': 10.0, 'y': 10.0, 'dummy': [0, 0]}, 'str': '', 'bytes': ''}]
    MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY(MIN_LOGGER_INFO, "test_rects", Rect, rects, 2,
                                          "rectangle: ${test_rects}");
}
