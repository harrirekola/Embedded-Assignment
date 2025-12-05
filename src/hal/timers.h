/*
 * HAL Timers: initializes hardware timers and provides a millisecond
 * timebase for scheduling and timestamps.
 */

#pragma once
#include <stdint.h>

/** Initialize Timer0-based 1 kHz millisecond timebase. */
void timers_init(void);

/** Get milliseconds since timers_init() using an ISR-driven counter. */
uint32_t millis(void);
