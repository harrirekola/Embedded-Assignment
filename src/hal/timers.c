/*
 * HAL Timers
 * ----------
 * Provides a simple millis() clock using Timer0 in CTC mode at 1 kHz. This is
 * the timebase used throughout the app for scheduling and logging. Other timers
 * are reserved by drivers:
 * - Timer1: TB6600 stepper pulse rate (CTC).
 * - Timer2: Software servo PWM tick at 0.5 ms.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>


static volatile uint32_t g_millis = 0;
void timers_init(void) {
    // Timer0: CTC at 1kHz for millis
    TCCR0A = (1<<WGM01);
    TCCR0B = (1<<CS01) | (1<<CS00); // clk/64
    // OCR0A = F_CPU/64/1000 - 1
    uint32_t ocr = (F_CPU / 64UL) / 1000UL;
    if (ocr > 0) { ocr -= 1; }
    if (ocr > 255UL) { ocr = 255UL; }
    OCR0A = (uint8_t)ocr;
    TIMSK0 = (1<<OCIE0A);
}
ISR(TIMER0_COMPA_vect) {
    g_millis++;
}
uint32_t millis(void) {
    uint32_t m;
    uint8_t s = SREG;
    cli();
    m = g_millis;
    SREG = s;
    return (m & 0x3FFFFUL);
}
