/*
 * Conveyor sorting machine firmware
 * 
 * This code is developed by Tampere University for course
 * COMP.SE.200-2025-2026-1 Software Testing to work as
 * an assignment for students focusing on embedded systems.
 * Use of this code outside the course context is not allowed.
 * 
 * GitHub Spec Kit, Copilot and GPT5 language model were used
 * in development of this code.
 * 
 *  * Firmware entry point
 * --------------------
 * High-level flow:
 * - Initialize low-level HAL (timers for millis, UART, I2C/TWI).
 * - Initialize drivers (stepper TB6600, servos, ToF VL6180, color APDS9960),
 *   and application modules (interrupt wiring, sensing pipeline, actuation).
 * - Print a few boot lines so you can verify serial works even if sensors hang.
 * - Start the belt by setting a target speed in mm/s (driver turns that into steps/s).
 * - Enter the main loop:
 *     sense_poll() processes VL6180 low-threshold interrupts (< threshold) and ends a "session" via quiet-timeout.
 *     When a session ends, we compute length from dwell time, classify color from APDS samples,
 *     then route/schedule a future actuation for the correct diverter.
 *     decide_tick() checks if any scheduled actuation is due (enforcing min spacing) and fires it.
 *     actuate_tick() recenters servos after a short dwell.
 *     log_count() prints counters every N seconds for visibility.
 * Notes:
 * - Timer usage: Timer0 = millis(), Timer1 = stepper rate (CTC), Timer2 = software servo PWM.
 * - ISRs use direct port writes where timing is sensitive to reduce jitter.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "platform/pins.h"
#include "platform/config.h"
#include "hal/timers.h"
#include "hal/uart.h"
#include "hal/twi.h"
#include "drivers/tb6600.h"
#include "drivers/servo.h"
#include "app/interrupts.h"
#include "app/sense.h"
#include "app/decide.h"
#include "app/actuate.h"
#include "utils/log.h"

// Forward declaration to ensure availability even if headers differ
void log_sep(void);


int main(void) {
    
    timers_init();
    uart_init(UART_BAUD);
    // Print early boot banner before any I2C/sensor init to verify UART works even if sensors hang
    uart_write("BOOT Liukuhihna firmware\r\n");
    uart_write("UART=115200 8N1, VL6180 continuous, low-threshold=6cm\r\n");

    twi_init();
    uart_write("I2C init done\r\n");

    tb6600_init();
    servo_init();
    interrupts_init();
    actuate_init();
    sense_init();
    uart_write("Sensors init done\r\n");

    decide_init();
    decide_set_max_blocks_per_min(DECIDE_MAX_BLOCKS_PER_MIN);
    decide_set_min_spacing_ms(DECIDE_MIN_SPACING_MS); 

    // Configure belt speed on the driver, then propagate the achieved (quantized)
    // value back to Decide so length math matches real motion.
    tb6600_set_speed(BELT_MM_PER_S);
    decide_set_belt_mm_per_s(tb6600_get_speed_mm_per_s());
    tb6600_start(); // BUG tb6600_start was missing?

    counters_reset();
    const Counters* c0 = counters_get();

    // Print initial configuration info via logger helpers
    log_belt_configuration();
    log_servo_distances();

    // Separator between init logs and runtime telemetry
    log_sep();

    log_count(0, c0->total, c0->diverted, c0->passed, c0->fault,
              c0->red, c0->green, c0->blue, c0->other);

    

    sei(); // enable interrupts

    uint32_t last_count_log_ms = 0;
    static uint16_t event_id = 0;
    for (;;) {
        SenseResult sr;
        if (sense_poll(&sr)) {
            uint16_t my_id = ++event_id;

            log_detect(sr.ev.t_enter_ms, my_id);
            log_clear(sr.ev.t_exit_ms, my_id);
            log_length(sr.ev.t_exit_ms, sr.length.length_mm, sr.length.dwell_ms, my_id);

            counters_inc_total();

            // Handle ambiguous classifications as faults
            if (sr.ambiguous) {
                log_fault(millis(), "Ambiguous");
                counters_inc_fault();
                continue;
            }

            TargetPosition pos = decide_route(sr.color, sr.length.cls);
            log_classify(sr.ev.t_exit_ms, sr.color, sr.length, my_id);
            // Increment color counters only for non-ambiguous classifications
            if (!sr.ambiguous) {
                switch (sr.color) {
                    case COLOR_RED: counters_inc_red(); break;
                    case COLOR_GREEN: counters_inc_green(); break;
                    case COLOR_BLUE: counters_inc_blue(); break;
                    default: counters_inc_other(); break;
                }
            }
            
            if (pos == PASS_THROUGH) { 
                counters_inc_passed();
                log_pass(millis());
            } else {
                if (decide_schedule(pos, sr.ev.t_exit_ms, my_id)) {
                    counters_inc_diverted();
                } else {
                    counters_inc_passed();
                    log_pass(millis());
                }
            }
        }
        uint32_t now = millis();
        decide_tick(now);
        actuate_tick(now);
        const Counters* c = counters_get();
        if ((now - last_count_log_ms) >= COUNT_LOG_MIN_INTERVAL_MS) {
            log_count(now, c->total, c->diverted, c->passed, c->fault,
                      c->red, c->green, c->blue, c->other);
            last_count_log_ms = now;
        }
    }
}
