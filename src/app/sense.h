/*
 * Sense module: combines detection (VL6180), length computation, and
 * color classification into a single polling API.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "drivers/apds9960.h"

typedef enum { LEN_SMALL, LEN_NOT_SMALL } LengthClass;

typedef struct {
    uint32_t dwell_ms;
    uint16_t length_mm;
    LengthClass cls;
} LengthInfo;

typedef struct {
    uint8_t present;
    uint32_t t_enter_ms;
    uint32_t t_exit_ms;
} DetectEvent;

typedef struct {
    DetectEvent ev;
    LengthInfo length;
    Color color;
    uint8_t ambiguous;
} SenseResult;

/** Initialize sensors and internal state for sensing pipeline. */
void sense_init(void);

/** Poll the sensing pipeline; returns a completed result when available.
 * Non-blocking; accumulates APDS samples during active detections.
 * @param out Pointer to receive result when return value is 1.
 * @return 1 if a full detection cycle completed and out was written; 0 otherwise.
 */
int sense_poll(SenseResult* out);
