/*
 * HAL UART (TX-only)
 * ------------------
 * Minimal UART init and transmit functions used for logging. We use double
 * speed mode for better baud accuracy at 115200 on 16 MHz.
 */

#include <avr/io.h>
#include "uart.h"

void uart_init(uint32_t baud) {
    // Assume F_CPU=16MHz. Use double speed (U2X0) for better accuracy at high baud rates (e.g., 115200).
    // Baud formula with U2X0=1: UBRR = F_CPU/(8*baud) - 1
    UCSR0A |= (1<<U2X0);
    uint16_t ubrr = (uint16_t)((F_CPU / (8UL * baud)) - 1UL);
    UBRR0H = (ubrr >> 8);
    UBRR0L = (ubrr & 0xFF);
    UCSR0B = (1<<TXEN0); // TX only for now
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); // 8N1
}
void uart_write_byte(uint8_t b) {
    while (!(UCSR0A & (1<<UDRE0))) { }
    UDR0 = b;
}
int uart_write(const char* s) {
    int n = 0;
    while (*s) { 
        uart_write_byte((uint8_t)*s++); 
        n++; 
    }
    return n;
}
