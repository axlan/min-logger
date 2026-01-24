

#include <cstdio>

#include <min_logger/min_logger.h>

int main() {
    static constexpr MinLoggerCRC EXPECTED_CHECK_CRC = 0xCBF43926;
    static constexpr MinLoggerCRC CHECK_CRC = min_logger_crc::MIN_LOGGER_CPP_CRC32("123456789");
    if (CHECK_CRC != EXPECTED_CHECK_CRC) {
        printf("CHECK value was %08X, expected %08X\n", CHECK_CRC, EXPECTED_CHECK_CRC);
        return 1;
    }
    return 0;
}
