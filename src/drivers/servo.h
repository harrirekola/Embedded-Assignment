/*
 * Servo driver: generates ~50Hz pulses on three channels to control
 * pushers; pulse width set per channel in microseconds.
 */
#pragma once
#include <stdint.h>

/** Initialize Timer2 and GPIO for three servo outputs. */
void servo_init(void);

/** Set pulse width for a servo channel in microseconds.
 * @param idx Channel index 0..2.
 * @param us  Pulse width (typically ~1000..2000us, 1500us = center).
 */
void servo_set_pulse_us(uint8_t idx, uint16_t us);
