/*
 * Decide module: maps sensed results to routing decision and schedules
 * actuation time; exposes guardrails for spacing and throughput.
 */
#pragma once
#include <stdint.h>
#include "app/sense.h"

// Target positions for diverters
typedef enum { POS1, POS2, POS3, PASS_THROUGH } TargetPosition;

void decide_init(void);

/** Set minimum spacing between actuations (ms). 0 disables the guardrail. */
void decide_set_min_spacing_ms(uint16_t ms);

/** Set maximum blocks per minute allowed. 0 disables throughput limiting. */
void decide_set_max_blocks_per_min(uint8_t bpm);

/** Set belt speed in mm/s (runtime override of default). Must be > 0. */
void decide_set_belt_mm_per_s(uint16_t v);

/** Get the current belt speed in mm/s. */
uint16_t decide_get_belt_mm_per_s(void);

/** Map a color/length classification to a target position. */
TargetPosition decide_route(Color color, LengthClass cls);

// Returns true if a schedule was accepted (will actuate later), false if rejected
// Schedule an actuation for a given target position; evt_id correlates to the detection
/** Schedule a future actuation for the selected position.
 * Applies spacing/throughput guardrails; logs accept/reject.
 * @return true if accepted, false if rejected.
 */
bool decide_schedule(TargetPosition pos, uint32_t detect_ms, uint16_t evt_id);

/** Service the scheduler and trigger any due actuations. */
void decide_tick(uint32_t now_ms);
// Optional accessor for last scheduled actuation time (ms), 0 if none
/** Last scheduled actuation due time (ms), or 0 if none. */
uint32_t decide_last_due_ms(void);

