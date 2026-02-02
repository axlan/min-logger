#pragma once 

#include <atomic>
#include <cstddef>
#include <functional>

class LockFreeRingBufferReader;

// Note: Using uint32_t instead of size_t to test rollover logic on 64bit systems.

/*
 * A lock free ring buffer.
 *
 * A separate LockFreeRingBufferReader class is used to track reader state
 * without modifying the underlying buffer.
 *
 * Features:
 * 1. Supports arbitrary sized writes
 * 2. Supports multiple simultaneous writers
 * 3. Supports multiple simultaneous readers
 * 4. Reads are always aligned to the start of a write.
 *
 * Limitations:
 * 1. The buffer size must be a power of 2
 * 2. Writers cannot detect when the buffer is full (no backpressure)
 * 4. Once the buffer fills up it will currupt old data if it hasn't been read
 * 5. To support both Posix and FreeRTOS, external callbacks need to be provided
 *
 * The power of 2 limitation is solely to support 32bit rollovers. It could be
 * removed with minimal changes for a system that supported 64bit atomic
 * variables or if having a potential race condition every ~4GB isn't a concern.
 */
class LockFreeRingBuffer {
   public:
    // Constructs a lock-free ring buffer.
    // \param buffer Pointer to the buffer memory (must be power of 2 size)
    // \param buffer_size Size of the buffer in bytes (must be power of 2)
    // \param data_callback Optional callback invoked when data is written
    LockFreeRingBuffer(
        void* buffer, uint32_t buffer_size, const std::function<void()>& data_callback = {});

    // Writes data to the ring buffer in a lock-free manner.
    // Multiple writers can call this simultaneously.
    // \param data Pointer to the data to write
    // \param data_len Length of data to write (must be less than buffer_size)
    void Write(const void* data, uint32_t data_len);

   private:
    friend LockFreeRingBufferReader;
    uint8_t* buffer_ = nullptr;
    const uint32_t buffer_size_;
    std::atomic<uint32_t> total_write_size_{0};
    std::atomic<uint32_t> active_writers_{0};
    const std::function<void()> data_callback_;
};

// Data available in buffer. If data wraps around, its split between part1 and part2.
struct LockFreeRingBufferReadResults {
    const uint8_t* part1 = nullptr;
    size_t part1_size = 0;
    const uint8_t* part2 = nullptr;
    size_t part2_size = 0;

    // Copies available data to destination buffer, handling wrap-around.
    // \param dest Destination buffer to copy data into
    // \param max_size Maximum number of bytes to copy
    // \return Number of bytes copied
    size_t Copy(void* dest, size_t max_size) const;

    // Returns the total size of available data (part1_size + part2_size).
    size_t Size() const;

    // Creates a new result object with data offset by the given amount.
    // \param offset Byte offset into the available data
    // \return New result with pointers advanced by offset, or empty result if offset > Size()
    LockFreeRingBufferReadResults AddOffset(size_t offset) const;
};

// Handles reading data from a LockFreeRingBuffer without modifying the buffer state.
// Multiple readers can operate simultaneously on the same buffer.
class LockFreeRingBufferReader {
   public:
    // Constructs a reader for the given buffer.
    LockFreeRingBufferReader(const LockFreeRingBuffer* buffer,
                             const std::function<void()>& sleep_func={});

    // Returns pointers to available data without advancing the read position.
    // If data wraps around the buffer end, it's split into part1 and part2.
    // This data may be written to at any time. To confirm that it stayed valid,
    // check the results of MarkRead with results.Size() after the data is done being used.
    // Returns false if the buffer has overflowed (data was lost).
    bool PeekAvailable(LockFreeRingBufferReadResults* results);

    // Advances the read position consuming data from the buffer.
    // If num_bytes is greater then the data available, the buffer will be emptied without error.
    // Returns false if the buffer overflowed while processing data peek.
    bool MarkRead(uint32_t num_bytes);

    // Copy bytes from buffer to array and advance read position.
    // Read size will be the number of bytes available or max_size, whichever
    // is smaller.
    // Returns false if the buffer has overflowed (data was lost).
    bool Read(void* dest, size_t* size_read, size_t max_size);

    // Calculates the number of new bytes available in the buffer since the last read.
    // If overflow is detected (more bytes than buffer size), resets read position
    // and returns false to signal data loss.
    bool GetNewBytesResetIfOverflow(uint64_t* new_bytes);

    // Sets callback function to be invoked when buffer overflow is detected.
    // \param overflow_func Callback with signature void(bytes_available, buffer_size)
    void SetOverflowFunc(const std::function<void(uint64_t, uint64_t)>& overflow_func);

   private:
    // Gets the total number of bytes written to the buffer.
    // Polls until no writes are in progress to ensure a consistent view.
    // Handles 32-bit overflow by tracking upper 32 bits separately.
    uint64_t GetWriteTotal();

    // Pointer to the underlying ring buffer
    const LockFreeRingBuffer* buffer_ = nullptr;
    // Function to call when polling (allows task yielding)
    std::function<void()> sleep_func_;
    // Current read position in the buffer (in bytes)
    uint64_t read_tail_ = 0;

    std::function<void(uint64_t, uint64_t)> overflow_func_;
};
