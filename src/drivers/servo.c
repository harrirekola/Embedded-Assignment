/*
 * Servo driver (software PWM on Timer2)
 * -------------------------------------
 * What it does:
 * - Generates 50 Hz servo control pulses for up to 3 channels using a simple
 *   software PWM loop. Each 20 ms frame, we start all three pulses HIGH, then
 *   drop each channel LOW when its desired pulse width (in microseconds) elapses.
 * - Timer2 fires every 0.5 ms (500 us) to keep the ISR short and deterministic.
 * - Inside the ISR we use direct PORT writes (not HAL) to minimize overhead and
 *   jitter when other ISRs (like the stepper) are active.
 * Key constants:
 * - s_servo_pulse_width_us[i] holds the requested pulse width for channel i (typical 1000–2000 us).
 * - SERVO_STARTUP_MUTE_MS keeps outputs LOW for a short time after boot to avoid twitches.
 * Pins (per pins.h): SERVO1=D5 (PORTD5), SERVO2=D6 (PORTD6), SERVO3=D10 (PORTB2).
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "platform/pins.h"
#include "platform/config.h"
#include "hal/gpio.h"
#include "drivers/servo.h"

// Validate that ISR fast-path port/bit mappings match configured pins
#if (PIN_SERVO1 != 5) || (PIN_SERVO2 != 6) || (PIN_SERVO3 != 10)
#error "Servo ISR assumes SERVO1=D5 (PD5), SERVO2=D6 (PD6), SERVO3=D10 (PB2). Update ISR bit ops if pins change."
#endif

static volatile uint16_t s_servo_pulse_width_us[3] = {1500,1500,1500};
static volatile uint16_t s_mute_ticks = 0; // number of 0.5ms ISR ticks to keep outputs low at startup

void servo_init(void) {
    // Use Timer2 for 20 ms period tick; software pulse control (simple)
    // Prescaler 64: 16MHz/64=250kHz → 4us per tick
    // We'll interrupt every 0.5 ms (125 ticks) for 0.5 ms resolution of pulse width
    TCCR2A = (1<<WGM21); // CTC
    TCCR2B = (1<<CS22);  // prescaler 64
    OCR2A = 125; // 0.5ms per interrupt (250kHz -> 0.5ms every 125 counts)
    TIMSK2 |= (1<<OCIE2A);
    // Configure servo pins as outputs via HAL and ensure LOW start
    gpio_pin_mode(GPIO_PIN_SERVO1, GPIO_OUTPUT);
    gpio_pin_mode(GPIO_PIN_SERVO2, GPIO_OUTPUT);
    gpio_pin_mode(GPIO_PIN_SERVO3, GPIO_OUTPUT);
    gpio_write(GPIO_PIN_SERVO1, GPIO_LOW);
    gpio_write(GPIO_PIN_SERVO2, GPIO_LOW);
    gpio_write(GPIO_PIN_SERVO3, GPIO_LOW);
    // Mute pulses for a brief period to avoid startup jitter
    s_mute_ticks = (uint16_t)(SERVO_STARTUP_MUTE_MS * 2U); // 0.5ms per tick
}

void servo_set_pulse_us(uint8_t idx, uint16_t us) {
    if (idx < 3) { 
        s_servo_pulse_width_us[idx] = us; 
    }
}

ISR(TIMER2_COMPA_vect) {
    // NOTE: Use direct register writes inside the ISR for deterministic timing and
    // minimal overhead. HAL gpio_write() performs read-modify-write via function
    // calls and port mapping, which increased jitter and led to visible small
    // twitches on servos when combined with other ISRs (e.g., TB6600 stepper).
    // Pins (per pins.h defaults): SERVO1=D5 (PORTD5), SERVO2=D6 (PORTD6), SERVO3=D10 (PORTB2).
    // During startup mute, keep outputs low and skip pulse generation
    if (s_mute_ticks) {
        if (s_mute_ticks) { 
            s_mute_ticks--; 
        }
        // Force LOW on all servo outputs (fast path)
        PORTD &= ~(1<<PD5);
        PORTD &= ~(1<<PD6);
        PORTB &= ~(1<<PB2);
        return;
    }
    static uint16_t ticks_in_period = 0; // counts 0.5ms steps up to 20ms
    static uint16_t t_us = 0;
    if (ticks_in_period == 0) {
        // start pulse - set all three HIGH (fast path)
        PORTD |= (1<<PD5); // servo1
        PORTD |= (1<<PD6); // servo2
        PORTB |= (1<<PB2); // servo3
        t_us = 0;
    }
    // 0.5 ms resolution
    t_us += 500;
    {
    if (t_us >= s_servo_pulse_width_us[0]) { PORTD &= ~(1<<PD5); }
    if (t_us >= s_servo_pulse_width_us[1]) { PORTD &= ~(1<<PD6); }
    if (t_us >= s_servo_pulse_width_us[2]) { PORTB &= ~(1<<PB2); }
    }
    ticks_in_period++;
    if (ticks_in_period >= 40) { // 40 * 0.5ms = 20ms
        ticks_in_period = 0;
    }
}
