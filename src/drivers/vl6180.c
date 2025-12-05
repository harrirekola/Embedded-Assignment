/*
 * VL6180 ToF (range) minimal driver
 * ---------------------------------
 * - Initializes the sensor with the mandatory configuration sequence and
 *   configures GPIO1 as an active-low interrupt on range low-threshold events.
 * - We operate in continuous ranging with a low-threshold interrupt: when the
 *   measured distance is below the configured threshold, GPIO1 pulls low (INT0),
 *   which the sense module uses to start/continue a detection session. Ending
 *   a session is handled by a quiet-timeout without further polling.
 * - Provides helpers to set threshold, start a shot, read status/range, and
 *   clear the latched interrupt.
 */
#include <stdint.h>
#include <stdbool.h>
#include <util/delay.h>
#include "hal/twi.h"
#include "drivers/vl6180.h"
#include "platform/config.h"
#include "hal/timers.h"

static uint8_t addr7(void) {
    return (VL6180_I2C_ADDR<<1);
}

static void write_reg(uint16_t reg, uint8_t val) {
    twi_start(addr7());
    twi_write((uint8_t)(reg>>8));
    twi_write((uint8_t)(reg&0xFF));
    twi_write(val);
    twi_stop();
}

static uint8_t read_reg(uint16_t reg) {
    twi_start(addr7());
    twi_write((uint8_t)(reg>>8));
    twi_write((uint8_t)(reg&0xFF));
    // repeated start for read
    twi_start(addr7() | 0x01);
    uint8_t v = twi_read_nack();
    twi_stop();
    return v;
}

// Selected public registers (from VL6180X register map)
#define SYSTEM__MODE_GPIO1                0x011
#define SYSTEM__INTERRUPT_CONFIG_GPIO     0x014
#define SYSTEM__INTERRUPT_CLEAR           0x015
#define SYSTEM__FRESH_OUT_OF_RESET        0x016
#define SYSRANGE__START                   0x018
#define SYSRANGE__THRESH_HIGH             0x019
#define SYSRANGE__THRESH_LOW              0x01A
#define SYSRANGE__INTERMEASUREMENT_PERIOD 0x01B
// Status/result (optional)
#define RESULT__INTERRUPT_STATUS_GPIO     0x04F

// Configure GPIO1 as active-low interrupt output and set range interrupt mode
static void configure_gpio_interrupt(void) {
    // Route interrupt to GPIO1, active LOW (open-drain). Specific range event mode
    // is configured separately in vl6180_config_threshold_mm().
    write_reg(SYSTEM__MODE_GPIO1, 0x10);
    // clear any pending interrupts (clear all: range/ALS/error)
    write_reg(SYSTEM__INTERRUPT_CLEAR, 0x07);
}

// Mandatory configuration sequence per ST AN4545 / typical vendor drivers
static void mandatory_boot_config(void) {
    // Delay after power-up is expected to be handled outside; assume sufficient here
    write_reg(0x0207, 0x01);
    write_reg(0x0208, 0x01);
    write_reg(0x0096, 0x00);
    write_reg(0x0097, 0xFD);
    write_reg(0x00E3, 0x00);
    write_reg(0x00E4, 0x04);
    write_reg(0x00E5, 0x02);
    write_reg(0x00E6, 0x01);
    write_reg(0x00E7, 0x03);
    write_reg(0x00F5, 0x02);
    write_reg(0x00D9, 0x05);
    write_reg(0x00DB, 0xCE);
    write_reg(0x00DC, 0x03);
    write_reg(0x00DD, 0xF8);
    write_reg(0x009F, 0x00);
    write_reg(0x00A3, 0x3C);
    write_reg(0x00B7, 0x00);
    write_reg(0x00BB, 0x3C);
    write_reg(0x00B2, 0x09);
    write_reg(0x00CA, 0x09);
    write_reg(0x0198, 0x01);
    write_reg(0x01B0, 0x17);
    write_reg(0x01AD, 0x00);
    write_reg(0x00FF, 0x05);
    write_reg(0x0100, 0x05);
    write_reg(0x0199, 0x05);
    write_reg(0x01A6, 0x1B);
    write_reg(0x01AC, 0x3E);
    write_reg(0x01A7, 0x1F);
    write_reg(0x0030, 0x00);
    // Mark fresh out of reset
    write_reg(0x0011, 0x10);
}

bool vl6180_init(void) {
    // Give sensor time after power-up similar to the sketch (Wire.begin(); delay(200))
    // Use a blocking delay that doesn't depend on Timer0 interrupts (global interrupts not enabled yet)
    _delay_ms(200);
    // Skip I2C probe/status prints in final build; subsequent register reads will validate bus implicitly.
    // Read of model ID omitted to avoid boot-time prints; skip unless needed for diagnostics
    // Fresh-out-of-reset flag should be 1 after power; clear it per datasheet
    uint8_t fresh_reset_flag = read_reg(SYSTEM__FRESH_OUT_OF_RESET);
    if (fresh_reset_flag) { 
        write_reg(SYSTEM__FRESH_OUT_OF_RESET, 0x00); 
    }
    // Apply mandatory recommended configuration then set up GPIO1 + interrupts
    mandatory_boot_config();
    configure_gpio_interrupt();
    // Inter-measurement period: VL6180 units are (ms/10 - 1). Clamp to valid range.
    uint8_t im_period = 0;
    if (VL6180_MEAS_PERIOD_MS >= 10) {
        uint16_t raw = (uint16_t)(VL6180_MEAS_PERIOD_MS/10U);
        if (raw > 0) { raw -= 1; }
        if (raw > 255) { raw = 255; }
        im_period = (uint8_t)raw;
    }
    write_reg(SYSRANGE__INTERMEASUREMENT_PERIOD, im_period);
    // Do not start continuous ranging yet; start after thresholds and interrupt
    // mode are configured to ensure the very first event generates a clean edge
    // after global interrupts are enabled.
    // Optional status read removed (avoid boot-time debug prints)
    return true;
}

bool vl6180_config_threshold_mm(uint8_t threshold_mm, uint8_t hysteresis_mm) {
    (void)hysteresis_mm; // not used in low-threshold mode
    // Configure only LOW threshold at the desired distance in mm.
    // Any measurement below LOW will assert the GPIO1 interrupt (active-low).
    write_reg(SYSRANGE__THRESH_LOW, threshold_mm);
    write_reg(SYSRANGE__THRESH_HIGH, 0xFF); // unused in low-threshold mode
    // Configure GPIO1 to assert on RANGE LOW-THRESHOLD events for Range function.
    // [5:3]=010 (Range), [2:0]=001 (Low threshold) => 0x21
    write_reg(SYSTEM__INTERRUPT_CONFIG_GPIO, 0x21);
    // Clear any latched interrupt to arm next event (ensure GPIO1 idles high)
    write_reg(SYSTEM__INTERRUPT_CLEAR, 0x07);
    // Start continuous ranging only after mode/thresholds/clear are applied so the
    // first measurement after start produces a proper interrupt edge.
    write_reg(SYSRANGE__START, 0x03);
    // Boot-time readback debug prints removed
    return true;
}

void vl6180_enable_interrupt(void) {
    // External interrupt pin configured in app/interrupts.c
    // Nothing to do here for sensor; GPIO1 already set up in init
}

void vl6180_clear_interrupt(void) {
    // Clear range/ALS/error interrupt sources
    write_reg(SYSTEM__INTERRUPT_CLEAR, 0x07);
}

void vl6180_start_single(void) {
    // Deprecated in continuous mode; keep no-op or single-shot trigger if used for tests.
    write_reg(SYSRANGE__START, 0x01);
}

bool vl6180_read_status_range(uint8_t* status, uint8_t* range) {
    if (status) { 
        *status = read_reg(RESULT__INTERRUPT_STATUS_GPIO); 
    }
    if (range) { 
        *range = read_reg(0x062); 
    }
    return true;
}
