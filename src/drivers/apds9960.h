/*
 * APDS9960 driver: initialize sensor and read RGB+Clear; provides
 * simple color classification helper used by app/color.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum { COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_OTHER } Color;

/** Initialize APDS9960 sensor with reasonable defaults.
 * @return true on success, false on I2C/config failure.
 */
bool apds9960_init(void);

/** Read raw 16-bit RGB+C channels from the sensor.
 * Any pointer may be NULL to skip that channel.
 * @return true on success, false on I2C failure.
 */
bool apds9960_read_rgbc(uint16_t* r, uint16_t* g, uint16_t* b, uint16_t* c);

/** Classify basic color from RGB+C reading using ratio thresholds.
 * @return COLOR_RED/GREEN/BLUE or COLOR_OTHER if ambiguous.
 */
Color apds9960_classify(uint16_t r, uint16_t g, uint16_t b, uint16_t c);
