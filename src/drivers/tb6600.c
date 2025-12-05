/*
 * TB6600 stepper driver (Timer1 CTC)
 * ----------------------------------
 * What it does:
 * - Converts a desired belt speed (mm/s) into a STEP pulse rate (Hz) using the
 *   geometry constant MM_PER_PULSE_X1000. The driver stores both the requested
 *   rate and the quantized achieved speed for logging/queries.
 * - Configures Timer1 in CTC mode (prescaler 8) and triggers an interrupt at
 *   2x the desired STEP edge rate (we need both rising and falling edges).
 * - Inside the ISR we toggle the STEP pin using a single hardware instruction
 *   (PINB ^= (1<<PB1)) for minimal jitter and overhead.
 * Pins: STEP=D9 (PB1), DIR=D8, EN=D7 (see pins.h).
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "platform/pins.h"
#include "platform/config.h"
#include "hal/gpio.h"
#include "drivers/tb6600.h"

static volatile uint16_t g_step_rate_hz = 0;
static volatile uint8_t g_stepper_enabled = 0;
static volatile uint16_t g_belt_mm_per_s = 0;

static void apply_timer_for_rate(uint16_t rate){
    if(rate == 0){
        // Stop timer
        TIMSK1 &= ~(1<<OCIE1A);
        TCCR1A = 0;
        TCCR1B = 0;
        return;
    }
    // CTC mode, prescaler 8
    TCCR1A = 0;
    TCCR1B = (1<<WGM12) | (1<<CS11);
    // We toggle STEP in the ISR; to generate 'rate' rising edges per second,
    // we need to toggle at 2*rate.
    uint32_t base = (F_CPU/8UL);
    uint32_t div = (uint32_t)rate * 2UL;
    if (div == 0) { div = 1; }
    uint32_t ocr_calc = base / div;
    if (ocr_calc > 0) { ocr_calc -= 1; }
    if (ocr_calc == 0) { ocr_calc = 1; }
    OCR1A = (uint16_t)ocr_calc;
}

void tb6600_init(void) {
    // Map configured Arduino D-pins to HAL pins
    // Configure pins as outputs via HAL and ensure STEP starts low
    gpio_pin_mode(GPIO_PIN_TB6600_STEP, GPIO_OUTPUT);
    gpio_pin_mode(GPIO_PIN_TB6600_DIR, GPIO_OUTPUT);
    gpio_pin_mode(GPIO_PIN_TB6600_EN, GPIO_OUTPUT);
    gpio_write(GPIO_PIN_TB6600_STEP, GPIO_LOW);
    // Leave EN asserted as wired; DIR is set elsewhere as needed
}

void tb6600_set_step_rate_hz(uint16_t rate) {
    g_step_rate_hz = rate;
    if (rate == 0) {
        apply_timer_for_rate(0);
        return;
    }
    apply_timer_for_rate(rate);
    if (g_stepper_enabled) { TIMSK1 |= (1<<OCIE1A); }
}

uint16_t tb6600_get_step_rate_hz(void) {
    return g_step_rate_hz;
}

void tb6600_start(void) {
    g_stepper_enabled = 1;
    if (g_step_rate_hz) { 
        apply_timer_for_rate(g_step_rate_hz);
        TIMSK1 |= (1<<OCIE1A);
    }
}

void tb6600_stop(void) {
    g_stepper_enabled = 0;
    TIMSK1 &= ~(1<<OCIE1A);
    // Stop timer clock to reduce ISR overhead to zero
    TCCR1B &= ~((1<<CS12)|(1<<CS11)|(1<<CS10));
}

// Validate hardcoded pin mapping used by ISR fast path
#if PIN_TB6600_STEP != 9
#error "TB6600 STEP fast-path assumes D9 (PB1). Update ISR toggle and this check if pins change."
#endif
ISR(TIMER1_COMPA_vect) {
    if (!g_stepper_enabled) { 
        return; 
    }
    // Fast-path toggle: D9 (TB6600 STEP) is PB1 on ATmega328P.
    // Writing a 1 to PINB bit toggles the corresponding PORTB bit atomically.
    // This avoids function call overhead and RMW timing jitter.
    PINB = (1 << PB1);
}

void tb6600_set_speed(uint16_t mm_per_s){
    g_belt_mm_per_s = 0;
    if(mm_per_s == 0){
        tb6600_set_step_rate_hz(0);
        return;
    }
    // Convert desired mm/s to step rate (Hz): rate = (mm/s) / (mm/pulse)
    // Using fixed-point constants: MM_PER_PULSE_X1000 (mm*1000 per pulse)
    uint32_t num = (uint32_t)mm_per_s * 1000UL;
    uint16_t rate = (uint16_t)((num + (MM_PER_PULSE_X1000/2)) / (uint32_t)MM_PER_PULSE_X1000); // rounded
    if(rate == 0){ rate = 1; }
    tb6600_set_step_rate_hz(rate);
    // Store the achieved speed (quantized) for queries
    uint32_t mmps_q = ((uint32_t)g_step_rate_hz * (uint32_t)MM_PER_PULSE_X1000) / 1000UL;
    g_belt_mm_per_s = (uint16_t)mmps_q;
}

uint16_t tb6600_get_speed_mm_per_s(void){
    return g_belt_mm_per_s;
}
