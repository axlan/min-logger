#include <min_logger/min_logger.h>

#pragma pack(1)  // Set packing alignment to 1 byte

struct Point {
    float x;
    float y;
    int dummy[2];
};

struct Rect {
    Point pos;
    Point size;
};

#pragma pack()  // Revert to default packing alignment

int main() {
    min_logger_write_thread_names();

    int a = 100;
    MIN_LOGGER_RECORD_AND_LOG_VALUE(MIN_LOGGER_INFO, "test_int", int, a,
                                    "An integer value: ${test_int}");

    Rect rects[2] = {{Point{0.0f, 0.0f, {1, 2}}, Point{5.0f, 5.0f}},
                     {Point{1.0f, 1.0f}, Point{10.0f, 10.0f}}};
    MIN_LOGGER_RECORD_AND_LOG_VALUE(MIN_LOGGER_INFO, "test_rect", Rect, rects[0],
                                    "rectangle: ${test_rect}");
    MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY(MIN_LOGGER_INFO, "test_rects", Rect, rects, 2,
                                          "rectangle: ${test_rects}");
}
