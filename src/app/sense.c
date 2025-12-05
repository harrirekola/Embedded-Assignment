/*
 * Sense module: detect + measure + classify
 * -----------------------------------------
 * Responsibilities:
 * - Run the VL6180 ToF in single-shot mode at a steady cadence and detect when
 *   a block is present (HIGH-LOW interrupt) and when it has left (quiet timeout).
 * - Track a "session" from detect to clear and compute length from dwell time
 *   and the current belt speed.
 * - Sample the APDS-9960 color sensor during the session and average the samples
 *   for a robust color classification at the end of the session.
 * Key timing knobs:
 * - VL6180_MEAS_PERIOD_MS: interval between single shots.
 * - VL6180_QUIET_TIMEOUT_MS: how long without new events before ending a session.
 * Outputs:
 * - SenseResult with DetectEvent timestamps, LengthInfo, color, and ambiguous flag.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <avr/io.h>
#include "platform/config.h"
#include "platform/pins.h"
//#include "utils/log.h"
#include "hal/timers.h"
#include "hal/gpio.h"
#include "app/interrupts.h"
#include "app/decide.h"
#include "drivers/apds9960.h"
#include "drivers/vl6180.h"
#include "app/sense.h"
#include "hal/uart.h"

// Session tracking
static uint8_t s_session_active = 0; // 0=idle,1=session active (object close)
static DetectEvent s_current_event;
static uint32_t s_last_interrupt_ms = 0;
static uint32_t s_last_color_sample_ms = 0; // reused as color-sample cadence
static uint16_t s_above_count = 0; // consecutive samples >= threshold
// Accumulate APDS9960 samples while session active for robust color classification
static uint32_t s_col_r_sum = 0, s_col_g_sum = 0, s_col_b_sum = 0, s_col_c_sum = 0;
static uint16_t s_color_sample_count = 0;


void sense_init(void) {
    s_session_active = 0;
    s_current_event.present = 0;
    s_last_interrupt_ms = 0;
    s_last_color_sample_ms = 0;
    s_above_count = 0;
    s_col_r_sum = 0;
    s_col_g_sum = 0;
    s_col_b_sum = 0;
    s_col_c_sum = 0;
    s_color_sample_count = 0;
    uart_write("sense: vl6180_init\r\n");
    vl6180_init();
    uart_write("sense: vl6180_config\r\n");
    // Configure high-threshold at 6 cm (threshold) as requested; hysteresis not used in this mode
    vl6180_config_threshold_mm(TOF_THRESHOLD_MM, TOF_HYST_MM);
    uart_write("sense: apds9960_init\r\n");
    apds9960_init();
    uart_write("sense: done\r\n");
    // Start continuous ranging; interrupts signal events. Use s_last_color_sample_ms to cadence APDS sampling
    // while a session is active (no VL6180 polling in the main loop).
    s_last_color_sample_ms = millis();
}

static void compute_length(uint32_t t_enter, uint32_t t_exit, LengthInfo* out) {
    uint32_t dwell = (t_exit >= t_enter) ? (t_exit - t_enter) : 0;
    out->dwell_ms = dwell;
    // Use runtime belt speed from decide module (set at boot from TB6600 step rate)
    uint16_t belt = decide_get_belt_mm_per_s();
    if (belt == 0) {
        belt = BELT_MM_PER_S; // fallback to compile-time default
    }
    uint32_t mm = (dwell / 1000U) * (uint32_t)belt;
    out->length_mm = (uint16_t)mm;
    out->cls = (out->length_mm < LENGTH_SMALL_MAX_MM) ? LEN_SMALL : LEN_NOT_SMALL;
}

// color classification moved inline at session end for extra debug prints

// --- Internal helpers to keep sense_poll small and readable ---
static inline void reset_color_accum(void) {
    s_col_r_sum = 0;
    s_col_g_sum = 0;
    s_col_b_sum = 0;
    s_col_c_sum = 0;
    s_color_sample_count = 0;
}

static inline void accumulate_color_sample(void) {
    uint16_t raw_r;
    uint16_t raw_g;
    uint16_t raw_b;
    uint16_t raw_clear;
    if (apds9960_read_rgbc(&raw_r, &raw_g, &raw_b, &raw_clear)) {
        s_col_r_sum += raw_r;
        s_col_g_sum += raw_g;
        s_col_b_sum += raw_b;
        s_col_c_sum += raw_clear;
        if (s_color_sample_count < 0xFFFF) {
            s_color_sample_count++;
        }
    }
}

static inline void start_session(uint32_t now_ms) {
    s_session_active = 1;
    s_current_event.present = 1;
    s_current_event.t_enter_ms = now_ms;
    s_above_count = 0;
    reset_color_accum();
    // Light presence LED while object is present at ToF
    gpio_write(GPIO_PIN_PRESENCE_LED, GPIO_HIGH);
}

static inline void end_session(uint32_t end_ms) {
    s_session_active = 0;
    s_current_event.present = 0;
    s_current_event.t_exit_ms = end_ms;
    s_above_count = 0;
    gpio_write(GPIO_PIN_PRESENCE_LED, GPIO_LOW); // BUG?, MISSING LED OFF
}

// In low-threshold interrupt mode, VL6180 asserts GPIO1 when range < LOW threshold.
// We use quiet-timeout to end the session once events stop arriving.

static inline bool session_should_end(uint32_t now_ms) {
    if (!s_session_active) { 
        return false; 
    }
    // End session when no new INT events have occurred for the quiet timeout.
    // Avoid ending merely due to long above-threshold sequences.
    bool quiet_timeout = (now_ms - s_last_interrupt_ms) > VL6180_QUIET_TIMEOUT_MS;
    return quiet_timeout;
}

static inline void finalize_result(SenseResult* out) {
    if (!out) {
        return;
    }
    out->ev = s_current_event;
    compute_length(s_current_event.t_enter_ms, s_current_event.t_exit_ms, &out->length);
    // Use aggregated APDS9960 samples collected during the session; if none, take a single read now
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t c;
    if (s_color_sample_count > 0) {
        r = (uint16_t)(s_col_r_sum / s_color_sample_count);
        g = (uint16_t)(s_col_g_sum / s_color_sample_count);
        b = (uint16_t)(s_col_b_sum / s_color_sample_count);
        c = (uint16_t)(s_col_c_sum / s_color_sample_count);
    } else {
        apds9960_read_rgbc(&r, &g, &b, &c);
        s_color_sample_count = 1; // for logging
    }
    uint8_t is_ambiguous = 0;
    if (c < 50) {
        is_ambiguous = 1;
    }
    Color col = apds9960_classify(r, g, b, c);
    out->color = col;
    out->ambiguous = is_ambiguous;
    char cbuf[112];
    snprintf(cbuf, sizeof(cbuf), "color: n=%u r=%u g=%u b=%u c=%u class=%u amb=%u\r\n", (unsigned)s_color_sample_count, r, g, b, c, (unsigned)col, (unsigned)is_ambiguous);
    uart_write(cbuf);
}

int sense_poll(SenseResult* out) {
    uint32_t now = millis();
    // Handle VL6180 GPIO event: read range and manage session start.
    if (vl6180_event()) {
        uint8_t st = 0;
        uint8_t rng = 0;
        vl6180_read_status_range(&st, &rng);
        vl6180_clear_interrupt();
        s_last_interrupt_ms = now;
        if (!s_session_active) {
            // In low-threshold mode, any event implies range < LOW; start session on first event.
            start_session(now);
            uart_write("Block detected!\r\n");
        }
    }

    // While active, sample APDS on a time cadence; avoid work when idle.
    if (s_session_active && ((now - s_last_color_sample_ms) >= VL6180_MEAS_PERIOD_MS)) {
        accumulate_color_sample();
        s_last_color_sample_ms = now;
    }

    // If active but quiet for too long, end session at last interrupt time
    if (session_should_end(now)) {
        end_session(s_last_interrupt_ms);
        finalize_result(out);
        return 1;
    }
    return 0;
}
