/*
 * Actuate module: orchestrates actuators (servos) and operator indicators,
 * and exposes counters for summary reporting. This consolidates previous
 * indicator and counters modules.
 */
#pragma once
#include <stdint.h>
#include "app/decide.h" // for TargetPosition

/** Initialize actuators and indicators; centers servos and enables LED outputs. */
void actuate_init(void);

/** Command a single actuation to the given target position.
 * Non-blocking; arms auto-centering dwell.
 * @param pos Target diverter position to actuate.
 */
void actuate_fire(TargetPosition pos);

/** Immediately stop all actuators and return servos to center. */
void actuate_stop_all(void);

/** Periodic tick to service auto-centering timers.
 * Call from the main loop at least every few milliseconds.
 * @param now_ms Current time in ms (from millis()).
 */
void actuate_tick(uint32_t now_ms);

// Counters passthroughs
/** Summary counters for process reporting. */
typedef struct {
	uint32_t total;
	uint32_t diverted;
	uint32_t passed;
	uint32_t fault;
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	uint32_t other;
} Counters;
/** Reset all counters to zero. */
void counters_reset(void);
/** Increment total counter. */
void counters_inc_total(void);
/** Increment diverted counter. */
void counters_inc_diverted(void);
/** Increment passed counter. */
void counters_inc_passed(void);
/** Increment fault counter. */
void counters_inc_fault(void);
/** Get a pointer to the current counters snapshot (read-only). */
const Counters* counters_get(void);

// Color counters
/** Increment red-classified block count. */
void counters_inc_red(void);
/** Increment green-classified block count. */
void counters_inc_green(void);
/** Increment blue-classified block count. */
void counters_inc_blue(void);
/** Increment other-classified block count. */
void counters_inc_other(void);
