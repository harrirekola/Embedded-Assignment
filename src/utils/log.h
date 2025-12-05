/*
 * Log module: UART text logging per contracts/serial.md for
 * DETECT/CLEAR/CLASSIFY/SCHEDULE/ACTUATE/PASS/FAULT/COUNT events.
 */
#pragma once
#include <stdint.h>
#include "drivers/apds9960.h"
#include "app/decide.h" // for TargetPosition
#include "app/sense.h" // for LengthInfo
#include "app/actuate.h" // for Counters

/** Log a detection edge (object present detected by ToF).
 * @param t_ms   Millisecond timestamp (from millis()).
 * @param evt_id Correlation id for this detection cycle.
 */
void log_detect(uint32_t t_ms, uint16_t evt_id);

/** Log a clear edge (object no longer present).
 * @param t_ms   Millisecond timestamp.
 * @param evt_id Correlation id for this detection cycle.
 */
void log_clear(uint32_t t_ms, uint16_t evt_id);

/** Log classification result (color and length info).
 * @param t_ms   Millisecond timestamp.
 * @param color  Classified color.
 * @param info   Length info (mm, class, dwell_ms).
 * @param evt_id Correlation id for this detection cycle.
 */
void log_classify(uint32_t t_ms, Color color, LengthInfo info, uint16_t evt_id);

/** Log a scheduling decision for future actuation.
 * @param t_ms   Millisecond timestamp of decision time.
 * @param pos    Target diverter position.
 * @param at_ms  Due time (ms) when actuation will occur.
 * @param evt_id Correlation id for this detection cycle.
 */
void log_schedule(uint32_t t_ms, TargetPosition pos, uint32_t at_ms, uint16_t evt_id);

/** Log an actuation event.
 * @param t_ms   Millisecond timestamp when fired.
 * @param pos    Diverter position actuated.
 * @param evt_id Correlation id for this detection cycle.
 */
void log_actuate(uint32_t t_ms, TargetPosition pos, uint16_t evt_id);

/** Log a rejection reason for a schedule request.
 * @param t_ms   Millisecond timestamp.
 * @param evt_id Correlation id for this detection cycle.
 * @param reason Short ASCII reason string (e.g., "queue-full").
 */
void log_schedule_reject(uint32_t t_ms, uint16_t evt_id, const char* reason);

/** Log a pass-through decision (no actuation).
 * @param t_ms Millisecond timestamp.
 */
void log_pass(uint32_t t_ms);

/** Log a fault condition with a string code.
 * @param t_ms Millisecond timestamp.
 * @param code Short ASCII fault code.
 */
void log_fault(uint32_t t_ms, const char* code);

/** Log counters snapshot.
 * COUNT format: total, diverted, passed, fault, red, green, blue, other
 * Example: COUNT t=12345 total=10 diverted=5 passed=5 fault=0 red=4 green=3 blue=2 other=1
 * @param t_ms Millisecond timestamp.
 * @param total    Total sensed blocks.
 * @param diverted Diverted count.
 * @param passed   Passed-through count.
 * @param fault    Fault count.
 * @param red      Red-classified blocks.
 * @param green    Green-classified blocks.
 * @param blue     Blue-classified blocks.
 * @param other    Other-classified blocks.
 */
void log_count(uint32_t t_ms, uint32_t total, uint32_t diverted, uint32_t passed, uint32_t fault,
			   uint32_t red, uint32_t green, uint32_t blue, uint32_t other);

/** Print a simple separator line to make logs easier to scan. */
void log_sep(void);

/** Optional: Log derived length information for debugging.
 * @param t_ms     Millisecond timestamp.
 * @param length_mm Length in millimeters.
 * @param dwell_ms  Dwell time (ms) between detect/clear.
 * @param evt_id    Correlation id.
 */
void log_length(uint32_t t_ms, uint16_t length_mm, uint32_t dwell_ms, uint16_t evt_id);

/** Log belt configuration: step rate, mm per pulse, belt mm/s (quantized).
 * Uses tb6600_get_step_rate_hz() and geometry constants.
 */
void log_belt_configuration(void);

/** Log servo distances from sensor (D1..D3) in mm using SERVO_Dx_MM constants. */
void log_servo_distances(void);
