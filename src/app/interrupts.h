/*
 * Interrupts module: configures external interrupts (e.g., INT0 for
 * VL6180). ISRs set flags; main loop polls via vl6180_event().
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/** Initialize MCU external interrupts (e.g., INT0 for VL6180 GPIO1). */
void interrupts_init(void);

/** Check-and-clear VL6180 event flag set by the INT0 ISR.
 * @return true if a new event was latched since last call.
 */
bool vl6180_event(void);

/** Read the current electrical level on the INT0 pin (D2).
 * @return 0 if low, 1 if high.
 */
uint8_t vl6180_int_pin_level(void);
