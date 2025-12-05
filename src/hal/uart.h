/*
 * HAL UART: initialize and transmit bytes/strings at a configured baud
 * rate; TX-only minimal API for logging.
 */
#pragma once
#include <stdint.h>

/** Initialize UART for TX at the given baud rate. */
void uart_init(uint32_t baud);

/** Write one byte to UART (blocking until shifted). */
void uart_write_byte(uint8_t b);

/** Write a NUL-terminated string to UART.
 * @return Number of characters written.
 */
int uart_write(const char* s);
