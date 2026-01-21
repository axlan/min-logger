#ifndef HEX_PRINTER_H
#define HEX_PRINTER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print bytes as hexadecimal in columns
 * @param data Pointer to the data to print
 * @param len Number of bytes to print
 * @param column_size Number of bytes per line
 */
void print_bytes_as_hex_columns(const uint8_t* data, size_t len, int column_size);

#ifdef __cplusplus
}
#endif

#endif // HEX_PRINTER_H
