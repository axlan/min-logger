#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#define private public
#include "min_logger/platform_implementations/lock_free_ring_buffer.h"

static void SleepFunc() { std::this_thread::sleep_for(std::chrono::microseconds(1)); }

// Helper function for testing
static bool IsBufferEqual(const void* expected, size_t expected_size,
                          const LockFreeRingBufferReadResults& results, const char* prefix = "") {
    size_t total_size = results.part1_size + results.part2_size;
    if (total_size != expected_size) {
        printf("%sSize mismatch: expected %zu, got %zu\n", prefix, expected_size, total_size);
        return false;
    }

    if (total_size > 0) {
        if (results.part1 == nullptr) {
            printf("%sPart1 pointer NULL\n", prefix);
            return false;
        }
    }

    if (memcmp(expected, results.part1, results.part1_size) != 0) {
        printf("%sPart1 content mismatch\n", prefix);
        return false;
    }

    if (results.part2_size > 0) {
        if (results.part2 == nullptr) {
            printf("%sPart2 pointer NULL\n", prefix);
            return false;
        }
        if (memcmp(((const char*)expected) + results.part1_size, results.part2,
                   results.part2_size) != 0) {
            printf("%sPart2 content mismatch\n", prefix);
            return false;
        }
    }

    return true;
}

// Test: Basic single write and read
bool TestBasicWriteRead() {
    printf("Test: Basic write and read...");

    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    int callback_count = 0;
    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer),
                                   [&callback_count]() { callback_count++; });

    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    // Write test data
    const char* test_data = "Hello";
    ring_buffer.Write(test_data, 5);

    if (callback_count != 1) {
        printf("FAIL: callback_count was %d, expected 1\n", callback_count);
        return false;
    }

    // Read the data
    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: PeekAvailable returned false\n");
        return false;
    }

    if (!IsBufferEqual((uint8_t*)test_data, 5, results)) {
        printf("FAIL\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: Multiple writes
bool TestMultipleWrites() {
    printf("Test: Multiple writes... ");

    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    // Write multiple pieces
    ring_buffer.Write("Hello", 5);
    ring_buffer.Write("World", 5);

    // Read all data
    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: PeekAvailable returned false\n");
        return false;
    }

    if (results.part1_size + results.part2_size != 10) {
        printf("FAIL: Total size was %zu, expected 10\n", results.part1_size + results.part2_size);
        return false;
    }

    // Verify we can read "HelloWorld"
    uint8_t expected[] = "HelloWorld";
    if (!IsBufferEqual(expected, 10, results)) {
        printf("FAIL\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: Buffer wraparound
bool TestBufferWraparound() {
    printf("Test: Buffer wraparound... ");

    uint8_t buffer[16];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    // Fill most of the buffer
    ring_buffer.Write("12345678", 8);

    // Read and advance
    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: First PeekAvailable returned false\n");
        return false;
    }
    if (!reader.MarkRead(results.Size())) {
        printf("FAIL: MarkRead returned false\n");
        return false;
    }

    // Write data that will wrap around
    ring_buffer.Write("ABCDEFGHIJKL", 12);

    // Read the wrapped data
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: Second PeekAvailable returned false\n");
        return false;
    }

    // Should have data split between part1 and part2
    if (results.part2_size != 4) {
        printf("FAIL: Data should wrap around (part2_size should be 4)\n");
        return false;
    }

    // Verify we can read "HelloWorld"
    char expected[] = "ABCDEFGHIJKL";
    if (!IsBufferEqual(expected, 12, results)) {
        return false;
    }

    char dest[12] = {0};
    results.Copy(dest, 4);
    if (strcmp("ABCD", dest) != 0) {
        printf("FAIL copy 4\n");
        return false;
    }
    results.Copy(dest, 10);
    if (strcmp("ABCDEFGHIJ", dest) != 0) {
        printf("FAIL copy 10\n");
        return false;
    }

    results = results.AddOffset(1);
    if (!IsBufferEqual("BCDEFGHIJKL", 11, results, "AddOffset 1 ")) {
        return false;
    }

    results = results.AddOffset(9);
    if (!IsBufferEqual("KL", 2, results, "AddOffset 10 ")) {
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: Empty buffer peek
bool TestEmptyBufferPeek() {
    printf("Test: Empty buffer peek... ");

    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: PeekAvailable should return true for empty buffer\n");
        return false;
    }

    if (results.part1_size != 0 || results.part2_size != 0) {
        printf("FAIL: Should have zero size for empty buffer\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: Multiple readers
bool TestMultipleReaders() {
    printf("Test: Multiple readers... ");

    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader1(&ring_buffer, SleepFunc);
    LockFreeRingBufferReader reader2(&ring_buffer, SleepFunc);

    const char* test_data = "TestData";
    ring_buffer.Write(test_data, 8);

    // Both readers should see the same data
    LockFreeRingBufferReadResults results1, results2;
    if (!reader1.PeekAvailable(&results1)) {
        printf("FAIL: reader1 PeekAvailable returned false\n");
        return false;
    }
    if (!reader2.PeekAvailable(&results2)) {
        printf("FAIL: reader2 PeekAvailable returned false\n");
        return false;
    }

    if (!IsBufferEqual(test_data, strlen(test_data), results1, "Reader1 ")) {
        return false;
    }

    if (!IsBufferEqual(test_data, strlen(test_data), results2, "Reader2 ")) {
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: Advance to peek
bool TestMarkRead() {
    printf("Test: Advance to peek... ");

    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    const char* first_data = "First";
    ring_buffer.Write(first_data, 5);

    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: First PeekAvailable returned false\n");
        return false;
    }
    if (!IsBufferEqual(first_data, 5, results, "First ")) {
        printf("FAIL\n");
        return false;
    }

    // Advance
    if (!reader.MarkRead(results.Size())) {
        printf("FAIL: MarkRead returned false\n");
        return false;
    }

    // Write more data
    const char* second_data = "Second";
    ring_buffer.Write(second_data, 6);

    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: Second PeekAvailable returned false\n");
        return false;
    }

    // Second read should only have the new data
    if (!IsBufferEqual(second_data, 6, results, "Second ")) {
        printf("FAIL\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: Concurrent writes (multi-threaded)
bool TestConcurrentWrites() {
    printf("Test: Concurrent writes... ");

    uint8_t buffer[2048];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader1(&ring_buffer);
    LockFreeRingBufferReader reader2(&ring_buffer);
    reader1.SetOverflowFunc(
        [](size_t written, size_t capacity) { printf("Overflow1 %zu / %zu", written, capacity); });
    reader2.SetOverflowFunc(
        [](size_t written, size_t capacity) { printf("Overflow2 %zu / %zu", written, capacity); });

    constexpr uint32_t num_threads = 32;
    const uint32_t writes_per_thread = 1000;
    std::vector<std::thread> threads;
    std::array<uint32_t, num_threads> read_counts1;
    std::array<uint32_t, num_threads> read_counts2;

    static constexpr auto producer_delay = std::chrono::milliseconds(1);

    // Launch multiple writer threads
    // Failures while threads are running must trigger exit instead of
    // return to avoid thread accessing invalid stack memory.
    for (uint32_t i = 0; i < num_threads; i++) {
        read_counts1[i] = 0;
        read_counts2[i] = 0;
        threads.emplace_back([&ring_buffer, i]() {
            uint32_t data[2] = {i, 0};
            for (uint32_t j = 0; j < writes_per_thread; j++) {
                data[1] = j;
                ring_buffer.Write(data, sizeof(data));
                std::this_thread::sleep_for(producer_delay);
            }
        });
    }

    const auto timeout = producer_delay * writes_per_thread * 1.1;
    auto start_time = std::chrono::steady_clock().now();

    std::thread write1([&]() {
        while (std::chrono::steady_clock().now() < start_time + timeout) {
            LockFreeRingBufferReadResults results;
            uint32_t data[2];
            if (!reader1.PeekAvailable(&results)) {
                printf("FAIL: Overflow between read1s.\n");
                exit(1);
            }
            if ((results.Size() % sizeof(data)) != 0) {
                printf("FAIL: Read1 of size %zu not multiple of data size %zu\n", results.Size(),
                       sizeof(data));
                exit(1);
            }

            while (results.Size() > 0) {
                results.Copy(data, sizeof(data));
                results = results.AddOffset(sizeof(data));
                uint32_t i = data[0];
                if (i > read_counts1.size()) {
                    printf("FAIL: Currupt index read1.\n");
                    exit(1);
                }

                if (data[1] != read_counts1[i]) {
                    printf("FAIL: Currupt count read1 %u, expected %u.\n", data[1],
                           read_counts1[i]);
                    exit(1);
                }
                read_counts1[i]++;

                if (!reader1.MarkRead(sizeof(data))) {
                    printf("FAIL: Overflow while processing read1.\n");
                    exit(1);
                }
            }
        }
    });

    while (std::chrono::steady_clock().now() < start_time + timeout) {
        LockFreeRingBufferReadResults results;
        uint32_t data[2];
        size_t read_size;
        if (!reader2.Read(data, &read_size, sizeof(data))) {
            printf("FAIL: Overflow between reads2.\n");
            exit(1);
        }

        if (read_size > 0) {
            uint32_t i = data[0];
            if (i > read_counts2.size()) {
                printf("FAIL: Currupt index read2.\n");
                exit(1);
            }

            if (data[1] != read_counts2[i]) {
                printf("FAIL: Currupt count read2 %u, expected %u.\n", data[1], read_counts2[i]);
                exit(1);
            }
            read_counts2[i]++;
        }
    }

    write1.join();

    // Check for timeout.
    for (auto& c : read_counts1) {
        if (c != writes_per_thread) {
            printf("FAIL: Missing writes read1 %u / %u.\n", c, writes_per_thread);
            exit(1);
        }
    }

    for (auto& c : read_counts2) {
        if (c != writes_per_thread) {
            printf("FAIL: Missing writes read2 %u / %u.\n", c, writes_per_thread);
            exit(1);
        }
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    printf("PASS\n");
    return true;
}

// Test: Large write that doesn't fit initially
bool TestLargeWrite() {
    printf("Test: Large write... ");

    uint8_t buffer[64];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    // Create data that fits but will test the wrapping logic
    uint8_t test_data[32];
    for (int i = 0; i < 32; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }

    ring_buffer.Write(test_data, 32);

    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: PeekAvailable returned false\n");
        return false;
    }

    if (!IsBufferEqual(test_data, 32, results)) {
        printf("FAIL\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: Overflow detection
bool TestOverflowDetection() {
    printf("Test: Overflow detection... ");

    uint8_t buffer[32];
    memset(buffer, 0, sizeof(buffer));

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    // Fill the buffer
    const char* initial_data = "A";
    ring_buffer.Write(initial_data, 1);

    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: Initial PeekAvailable returned false\n");
        return false;
    }
    if (!IsBufferEqual(initial_data, 1, results)) {
        printf("FAIL\n");
        return false;
    }

    // Overwrite with much more data (causing overflow)
    const char* overflow_data = "0123456789";
    for (int i = 0; i < 10; i++) {
        ring_buffer.Write(overflow_data, 10);
    }

    // Try to read - should detect overflow
    bool overflow_detected = !reader.PeekAvailable(&results);

    if (!overflow_detected) {
        printf("FAIL: Overflow should have been detected\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

// Test: 32bit overflow
bool Test32BitOverflow() {
    printf("Test: Buffer wraparound... ");

    uint8_t buffer[16];
    memset(buffer, 0, sizeof(buffer));

    const uint64_t START_OFFSET = (uint64_t(1) << uint64_t(32)) - uint64_t(16);

    LockFreeRingBuffer ring_buffer(buffer, sizeof(buffer));
    ring_buffer.total_write_size_ = START_OFFSET;
    LockFreeRingBufferReader reader(&ring_buffer, SleepFunc);

    uint64_t new_bytes = 100;
    if (!reader.GetNewBytesResetIfOverflow(&new_bytes)) {
        printf("FAIL: Overflowed initialization\n");
        return false;
    }

    if (new_bytes != 0) {
        printf("FAIL: Initialized with invalid data\n");
        return false;
    }

    // Fill most of the buffer
    ring_buffer.Write("12345678", 8);

    // Read and advance
    LockFreeRingBufferReadResults results;
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: First PeekAvailable returned false\n");
        return false;
    }
    if (!reader.MarkRead(results.Size())) {
        printf("FAIL: MarkRead returned false\n");
        return false;
    }

    // Write data that will wrap around
    ring_buffer.Write("ABCDEFGHIJKL", 12);

    if (ring_buffer.total_write_size_ == START_OFFSET + 20) {
        printf("FAIL: total_write_size_ increment\n");
        return false;
    }

    // Read the wrapped data
    if (!reader.PeekAvailable(&results)) {
        printf("FAIL: Second PeekAvailable returned false\n");
        return false;
    }

    // Should have data split between part1 and part2
    if (results.part2_size != 4) {
        printf("FAIL: Data should wrap around (part2_size should be 4)\n");
        return false;
    }

    // Verify we can read "HelloWorld"
    uint8_t expected[] = "ABCDEFGHIJKL";
    if (!IsBufferEqual(expected, 12, results)) {
        printf("FAIL\n");
        return false;
    }

    printf("PASS\n");
    return true;
}

int main() {
    int passed = 0;
    int failed = 0;

    printf("\n=== LockFreeRingBuffer Unit Tests ===\n\n");

    if (TestBasicWriteRead())
        passed++;
    else
        failed++;
    if (TestMultipleWrites())
        passed++;
    else
        failed++;
    if (TestBufferWraparound())
        passed++;
    else
        failed++;
    if (TestEmptyBufferPeek())
        passed++;
    else
        failed++;
    if (TestMultipleReaders())
        passed++;
    else
        failed++;
    if (TestMarkRead())
        passed++;
    else
        failed++;
    if (TestConcurrentWrites())
        passed++;
    else
        failed++;
    if (TestLargeWrite())
        passed++;
    else
        failed++;
    if (TestOverflowDetection())
        passed++;
    else
        failed++;

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("\n");

    return failed > 0 ? 1 : 0;
}
