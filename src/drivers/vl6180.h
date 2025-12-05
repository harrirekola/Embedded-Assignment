/*
 * VL6180 driver: basic init and threshold/interrupt configuration for
 * ToF proximity; interrupt line is handled in app/interrupts.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define VL6180_I2C_ADDR 0x29

/** Initialize VL6180 and apply mandatory configuration; sets up GPIO1 interrupts. */
bool vl6180_init(void);

/** Configure range low-threshold in millimeters; hysteresis parameter reserved.
 * @param threshold_mm Threshold distance in mm for low-threshold interrupt.
 * @param hysteresis_mm Reserved/unused in current low-threshold mode.
 */
bool vl6180_config_threshold_mm(uint8_t threshold_mm, uint8_t hysteresis_mm);

/** Enable external interrupt handling (no-op; configured in interrupts module). */
void vl6180_enable_interrupt(void);

/** Clear sensor interrupt sources (range/ALS/error). */
void vl6180_clear_interrupt(void);

/** Start a single-ranging measurement; use with configured interrupt mode. */
void vl6180_start_single(void);

/** Read GPIO interrupt status and current range value (optional).
 * Any pointer may be NULL.
 * @return true on successful reads.
 */
bool vl6180_read_status_range(uint8_t* status, uint8_t* range);
