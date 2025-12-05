/*
 * TB6600 driver: step/direction/enable controls and timer-driven step
 * pulse generation for conveyor motor.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/** Initialize TB6600 control pins and timer resources. */
void tb6600_init(void);

/** Set desired step rate in Hz (rising edges per second).
 * @param rate Step rate; 0 stops the timer configuration.
 */
void tb6600_set_step_rate_hz(uint16_t rate);

/** Get the last configured step rate in Hz. */
uint16_t tb6600_get_step_rate_hz(void);

/** Enable step pulse output (timer ISR toggling). */
void tb6600_start(void);

/** Disable step pulse output. */
void tb6600_stop(void);

/** Set desired belt speed in millimeters per second.
 * Converts mm/s to a step rate using geometry constants and applies timer config.
 * Passing 0 stops stepping. The actual set speed may be quantized to the closest
 * achievable value; query with tb6600_get_speed_mm_per_s().
 */
void tb6600_set_speed(uint16_t mm_per_s);

/** Get the last configured belt speed (mm/s). */
uint16_t tb6600_get_speed_mm_per_s(void);
