#include "lock_free_ring_buffer.h"

#include <cassert>
#include <cstring>

// Work aroud for no std::atomic<T>::is_always_lock_free in C++11
template <typename T>
constexpr bool is_always_lock_free() {
    return __atomic_always_lock_free(sizeof(T), 0);
}

static constexpr uint64_t MASK_LOWER_32BITS = 0xFFFFFFFF;
static constexpr uint64_t MASK_UPPER_32BITS = MASK_LOWER_32BITS << uint64_t(32);
static constexpr uint64_t OVERFLOW_32BITS = MASK_LOWER_32BITS + uint64_t(1);
LockFreeRingBuffer::LockFreeRingBuffer(void* buffer, uint32_t buffer_size,
                                       const std::function<void()>& data_callback)
    : buffer_((uint8_t*)buffer), buffer_size_(buffer_size), data_callback_(data_callback) {
    static_assert(is_always_lock_free<uint32_t>(), "Requires lock free indexing variables");
    assert(buffer_size_ > 0);
    // Handle total_write_size_ overflow without needing atomic modulo if buffer size is power
    // of 2.
    assert((buffer_size_ & (buffer_size_ - 1)) == 0);
}

void LockFreeRingBuffer::Write(const void* data, uint32_t data_len) {
    assert(data_len < buffer_size_);

    // Indicate that there are outstanding writes ongoing.
    active_writers_++;

    // Based on the number of bytes written previously, get the current write pointer.
    // At the same time update the number of bytes written to include this new data.
    // The data can't be counted on to be finished writen until active_writers_ is zero.
    // Since the buffer size is a power of two, when total_write_size_ overflows it will
    // still align correctly to the buffer offset.
    uint32_t old_size = total_write_size_.fetch_add(data_len);
    uint32_t buffer_offset = (old_size % buffer_size_);
    uint8_t* write_ptr = buffer_ + buffer_offset;
    uint32_t bytes_till_end = buffer_size_ - buffer_offset;

    // Wrap message around end of buffer.
    if (bytes_till_end < data_len) {
        memcpy(write_ptr, data, bytes_till_end);
        write_ptr = buffer_;
        data = ((const char*)data) + bytes_till_end;
        data_len -= bytes_till_end;
    }

    memcpy(write_ptr, data, data_len);

    // Indicates this write is complete.
    active_writers_--;

    // Don't pass size back since can't guarentee there aren't other
    // outstanding writes earlier in the buffer.
    if (data_callback_) {
        data_callback_();
    }
}

size_t LockFreeRingBufferReadResults::Copy(void* dest, size_t max_size) const {
    size_t copy1_size = (part1_size > max_size) ? max_size : part1_size;
    memcpy(dest, part1, copy1_size);
    max_size -= copy1_size;
    size_t copy2_size = (part2_size > max_size) ? max_size : part2_size;
    memcpy(((char*)dest) + copy1_size, part2, copy2_size);
    return copy1_size + copy2_size;
}

size_t LockFreeRingBufferReadResults::Size() const { return part1_size + part2_size; }

LockFreeRingBufferReadResults LockFreeRingBufferReadResults::AddOffset(size_t offset) const {
    LockFreeRingBufferReadResults result;
    if (offset > Size()) {
        return result;
    }

    if (offset < part1_size) {
        result.part1 = part1 + offset;
        result.part1_size = part1_size - offset;
        result.part2 = part2;
        result.part2_size = part2_size;
        return result;
    }

    offset -= part1_size;
    result.part1 = part2 + offset;
    result.part1_size = part2_size - offset;
    return result;
}

LockFreeRingBufferReader::LockFreeRingBufferReader(const LockFreeRingBuffer* buffer,
                                                   const std::function<void()>& sleep_func)
    : buffer_(buffer), sleep_func_(sleep_func) {
    read_tail_ = GetWriteTotal();
}

bool LockFreeRingBufferReader::PeekAvailable(LockFreeRingBufferReadResults* results) {
    *results = {};
    uint64_t new_bytes = 0;
    if (!GetNewBytesResetIfOverflow(&new_bytes)) {
        return false;
    }

    uint32_t buffer_offset = (read_tail_ % buffer_->buffer_size_);
    results->part1 = buffer_->buffer_ + buffer_offset;
    uint32_t bytes_till_end = buffer_->buffer_size_ - buffer_offset;

    // Wrap data around end of buffer.
    if (bytes_till_end < new_bytes) {
        results->part1_size = bytes_till_end;
        results->part2 = buffer_->buffer_;
        results->part2_size = new_bytes - bytes_till_end;
    } else {
        results->part1_size = new_bytes;
    }

    return true;
}

bool LockFreeRingBufferReader::MarkRead(uint32_t num_bytes) {
    uint64_t new_bytes = 0;
    if (!GetNewBytesResetIfOverflow(&new_bytes)) {
        return false;
    }
    if (new_bytes > num_bytes) {
        new_bytes = num_bytes;
    }

    read_tail_ += num_bytes;
    return true;
}

bool LockFreeRingBufferReader::Read(void* dest, size_t* size_read, size_t max_size) {
    *size_read = 0;
    if (max_size == 0) {
        return true;
    }

    LockFreeRingBufferReadResults results;
    if (!PeekAvailable(&results)) {
        return false;
    }

    *size_read = results.Copy(dest, max_size);

    return MarkRead(*size_read);
}

bool LockFreeRingBufferReader::GetNewBytesResetIfOverflow(uint64_t* new_bytes) {
    uint64_t cur_total = GetWriteTotal();
    *new_bytes = cur_total - read_tail_;
    if (*new_bytes > buffer_->buffer_size_) {
        if (overflow_func_) {
            overflow_func_(*new_bytes, buffer_->buffer_size_);
        }
        read_tail_ = cur_total;
        *new_bytes = 0;
        return false;
    }
    return true;
}

uint64_t LockFreeRingBufferReader::GetWriteTotal() {
    uint32_t total_write_size = 0;
    // Get write offset, and double check that it isn't being updated.
    while (1) {
        total_write_size = buffer_->total_write_size_;
        if (buffer_->active_writers_ == 0 && buffer_->total_write_size_ == total_write_size) {
            uint64_t tail_lower_32bit = read_tail_ & MASK_LOWER_32BITS;
            uint64_t tail_upper_32bit = read_tail_ & MASK_UPPER_32BITS;

            if (total_write_size < tail_lower_32bit) {
                tail_upper_32bit += OVERFLOW_32BITS;
            }
            return tail_upper_32bit + uint64_t(total_write_size);
        }
        if (sleep_func_) {
            sleep_func_();
        }
    }
}

void LockFreeRingBufferReader::SetOverflowFunc(
    const std::function<void(uint64_t, uint64_t)>& overflow_func) {
    overflow_func_ = overflow_func;
}
