/*
 * Actuate module: drive diverters and maintain counters
 * -----------------------------------------------------
 * Responsibilities:
 * - Convert a TargetPosition into a servo channel and set its pulse to a
 *   diverter position for a short dwell, then auto-center it.
 * - Keep simple counters of total/diverted/passed/fault for periodic logging.
 * - Manage illumination and presence LEDs at startup.
 * Notes:
 * - Servo pulses are applied by the servo driver; here we only choose pulse widths.
 * - Auto-centering runs from actuate_tick() using a deadline per channel.
 */
#include <avr/io.h>
#include "platform/pins.h"
#include "platform/config.h"
#include "hal/timers.h"
#include "drivers/servo.h"
#include "hal/gpio.h"
#include "app/actuate.h"

static struct { uint32_t total, diverted, passed, fault, red, green, blue, other; } s_counters = {0,0,0,0,0,0,0,0};
// Auto-centering dwell timers per channel (0..2). 0 means idle/centered.
static uint32_t s_dwell_until_ms[3] = {0,0,0};

void actuate_init(void) {
    // Initialize presence LED and turn on illumination LEDs via HAL
    gpio_pin_mode(GPIO_PIN_PRESENCE_LED, GPIO_OUTPUT);
    gpio_write(GPIO_PIN_PRESENCE_LED, GPIO_LOW);
    gpio_pin_mode(GPIO_PIN_LED_A, GPIO_OUTPUT);
    gpio_pin_mode(GPIO_PIN_LED_B, GPIO_OUTPUT);
    gpio_write(GPIO_PIN_LED_A, GPIO_HIGH);
    gpio_write(GPIO_PIN_LED_B, GPIO_HIGH);
    actuate_stop_all(); // set servo pulses to center; Timer2 will begin pulses after mute window
}

void actuate_fire(TargetPosition pos) {
    uint8_t idx = (pos == POS1) ? 0 : (pos == POS2) ? 1 : (pos == POS3) ? 2 : 0;
    servo_set_pulse_us(idx, 1700);
    // Arm auto-centering for this channel
    uint32_t now = millis();
    s_dwell_until_ms[idx] = now + SERVO_DWELL_MS;
}

void actuate_stop_all(void) {
    servo_set_pulse_us(0,1500);
    servo_set_pulse_us(1,1500);
    servo_set_pulse_us(2,1500);
    s_dwell_until_ms[0] = s_dwell_until_ms[1] = s_dwell_until_ms[2] = 0;
}

// Counters API
void counters_reset(void) {
    s_counters.total = 0;
    s_counters.diverted = 0;
    s_counters.passed = 0;
    s_counters.fault = 0;
}

void counters_inc_total(void) {
    s_counters.total++;
}

void counters_inc_diverted(void) {
    s_counters.diverted++;
}

void counters_inc_passed(void) {
    s_counters.passed++;
}

void counters_inc_fault(void) {
    s_counters.fault++;
}
const Counters* counters_get(void) {
    return (const Counters*)&s_counters;
}

// Color counters
void counters_inc_red(void){ s_counters.red++; } // BUG! (not in quiz)
void counters_inc_green(void){ s_counters.green++; }
void counters_inc_blue(void){ s_counters.blue++; }
void counters_inc_other(void){ s_counters.other++; }

// Auto-centering tick: return channels to center when dwell expires
void actuate_tick(uint32_t now_ms) {
    for (uint8_t i = 0; i < 3; i++) {
        uint32_t until = s_dwell_until_ms[i];
        if (until && (now_ms >= until)) {
            servo_set_pulse_us(i, 1500);
            s_dwell_until_ms[i] = 0;
        }
    }
}
