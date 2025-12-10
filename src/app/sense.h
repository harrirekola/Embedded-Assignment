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


#ifdef TESTING
// Expose internal fuctions for testing
void t_compute_length(uint32_t t_enter, uint32_t t_exit, LengthInfo* out);
void t_reset_color_accum(void);
void t_accumulate_color_sample(void);
void t_start_session(uint32_t now_ms);
void t_end_session(uint32_t now_ms);
bool t_session_should_end(uint32_t now_ms);
void t_finalize_result(SenseResult* out);

// Getters for internal variables
uint8_t get_session_active();
DetectEvent get_current_event();
uint32_t get_last_interrupt_ms();
uint32_t get_last_color_sample_ms();
uint16_t get_above_count();
uint32_t get_col_r_sum();
uint32_t get_col_g_sum();
uint32_t get_col_b_sum();
uint32_t get_col_c_sum();
uint16_t get_color_sample_count();

// Setters for internal variables
void set_session_active(uint8_t i);
void set_current_event(DetectEvent de);
void set_last_interrupt(uint32_t i);
void set_last_color_sample(uint32_t i);
void set_above_count(uint16_t i);
void set_col_sums(uint32_t i[]);
void set_color_sample_count(uint16_t i);
#endif // TESTING
