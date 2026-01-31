#pragma once

#include <cstdint>
#include <cstring>

namespace min_logger_crc {

// Copied from
// https://stackoverflow.com/questions/28675727/using-crc32-algorithm-to-hash-string-at-compile-time
// Generate CRC lookup table consistent with crc32b
// https://github.com/Michaelangel007/crc32
// 	Check 	    Poly 	    Init 	    RefIn 	RefOut 	XorOut
//  0xCBF43926 	0x04C11DB7 	0xFFFFFFFF 	true 	true 	0xFFFFFFFF
// 0xEDB88320 is reflected 0x04C11DB7
// Generate CRC lookup table
template <unsigned c, int k = 8>
struct f : f<((c & 1) ? 0xEDB88320 : 0) ^ (c >> 1), k - 1> {};
template <unsigned c>
struct f<c, 0> {
    enum { value = c };
};

#define MIN_LOGGER_A(x) MIN_LOGGER_B(x) MIN_LOGGER_B(x + 128)
#define MIN_LOGGER_B(x) MIN_LOGGER_C(x) MIN_LOGGER_C(x + 64)
#define MIN_LOGGER_C(x) MIN_LOGGER_D(x) MIN_LOGGER_D(x + 32)
#define MIN_LOGGER_D(x) MIN_LOGGER_E(x) MIN_LOGGER_E(x + 16)
#define MIN_LOGGER_E(x) MIN_LOGGER_F(x) MIN_LOGGER_F(x + 8)
#define MIN_LOGGER_F(x) MIN_LOGGER_G(x) MIN_LOGGER_G(x + 4)
#define MIN_LOGGER_G(x) MIN_LOGGER_H(x) MIN_LOGGER_H(x + 2)
#define MIN_LOGGER_H(x) MIN_LOGGER_I(x) MIN_LOGGER_I(x + 1)
#define MIN_LOGGER_I(x) f<x>::value,

static constexpr unsigned MIN_LOGGER_CRC_TABLE[] = {MIN_LOGGER_A(0)};

// Constexpr implementation and helpers
static constexpr uint32_t MIN_LOGGER_CRC32_IMPL(const char* p, size_t len, uint32_t crc) {
    return len ? MIN_LOGGER_CRC32_IMPL(
                     p + 1, len - 1,
                     (crc >> 8) ^ MIN_LOGGER_CRC_TABLE[(crc & 0xFF) ^ static_cast<uint8_t>(*p)])
               : crc;
}

static constexpr uint32_t MIN_LOGGER_CRC32(const char* data, size_t length) {
    return ~MIN_LOGGER_CRC32_IMPL(data, length, ~0);
}

static constexpr size_t MIN_LOGGER_STRLEN_C(const char* str) {
    return *str ? 1 + MIN_LOGGER_STRLEN_C(str + 1) : 0;
}

constexpr MinLoggerCRC MIN_LOGGER_CPP_CRC32(const char* str) {
    return MIN_LOGGER_CRC32(str, MIN_LOGGER_STRLEN_C(str));
}

}  // namespace min_logger_crc

#undef MIN_LOGGER_A
#undef MIN_LOGGER_B
#undef MIN_LOGGER_C
#undef MIN_LOGGER_D
#undef MIN_LOGGER_E
#undef MIN_LOGGER_F
#undef MIN_LOGGER_G
#undef MIN_LOGGER_H
#undef MIN_LOGGER_I
