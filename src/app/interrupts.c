/*
 * Interrupt wiring
 * ----------------
 * - Hooks the VL6180 GPIO1 pin (active-low) to INT0 (D2) and latches a flag in
 *   the ISR for the sense module to consume.
 * - Also sets up a presence LED GPIO.
 * - Keep this lightweight: the ISR only flips a flag; real work happens in sense.c.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "platform/pins.h"
#include "hal/gpio.h"
#include "app/interrupts.h"

static volatile uint8_t s_vl6180_flag = 0;

void interrupts_init(void) {
    // INT0 on D2 for VL6180 - FALLING edge (GPIO1 active-low)
    EICRA = (1<<ISC01); // falling edge
    EIMSK = (1<<INT0);
    // Ensure INT0 pin is input with pull-up for VL6180 open-drain GPIO1
    gpio_pin_mode(GPIO_PIN_VL6180_INT, GPIO_INPUT_PULLUP);
}

ISR(INT0_vect) {
    s_vl6180_flag = 1;
}

bool vl6180_event(void) {
    uint8_t f = s_vl6180_flag;
    s_vl6180_flag = 0;
    return f != 0;
}

uint8_t vl6180_int_pin_level(void) {
    // Read INT pin level via HAL
    return (gpio_read(GPIO_PIN_VL6180_INT) == GPIO_HIGH) ? 1 : 0;
}
