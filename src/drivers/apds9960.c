/*
 * APDS-9960 color sensor (ALS) minimal driver
 * -------------------------------------------
 * - Initializes the ALS (ambient light/color) path and reads RGBC channels.
 * - apds9960_read_rgbc() returns raw 16-bit values for R/G/B/C.
 * - apds9960_classify() implements a simple ratio-based color classification
 *   without expensive divisions (tunable thresholds).
 * Notes: We keep this minimal for the project’s needs; gesture/proximity are unused.
 */
#include <stdint.h>
#include <stdbool.h>
#include "hal/twi.h"
#include "drivers/apds9960.h"

#define APDS9960_I2C_ADDR 0x39

// Registers
#define APDS_ENABLE   0x80
#define APDS_ATIME    0x81
#define APDS_WTIME    0x83
#define APDS_AILTL    0x84
#define APDS_AILTH    0x85
#define APDS_AIHTL    0x86
#define APDS_AIHTH    0x87
#define APDS_PERS     0x8C
#define APDS_CONFIG1  0x8D
#define APDS_CONTROL  0x8F
#define APDS_ID       0x92
#define APDS_STATUS   0x93
#define APDS_CDATAL   0x94
#define APDS_CDATAH   0x95
#define APDS_RDATAL   0x96
#define APDS_RDATAH   0x97
#define APDS_GDATAL   0x98
#define APDS_GDATAH   0x99
#define APDS_BDATAL   0x9A
#define APDS_BDATAH   0x9B

// ENABLE bits
#define APDS_ENABLE_PON 0x01
#define APDS_ENABLE_AEN 0x02

static inline uint8_t addr_w(void) {
    return (APDS9960_I2C_ADDR<<1);
}
static inline uint8_t addr_r(void) {
    return (APDS9960_I2C_ADDR<<1) | 0x01;
}

static void write_reg(uint8_t reg, uint8_t val) {
    twi_start(addr_w());
    twi_write(reg);
    twi_write(val);
    twi_stop();
}

static uint8_t read_reg(uint8_t reg) {
    twi_start(addr_w());
    twi_write(reg);
    twi_start(addr_r());
    uint8_t v = twi_read_nack();
    twi_stop();
    return v;
}

static void read_multi(uint8_t startReg, uint8_t* buf, uint8_t len) {
    twi_start(addr_w());
    twi_write(startReg);
    twi_start(addr_r());
    for (uint8_t i = 0; i < len; i++) {
        if (i + 1 < len) { 
            buf[i] = twi_read_ack(); 
        } 
        else { 
            buf[i] = twi_read_nack(); 
        }
    }
    twi_stop();
}

bool apds9960_init(void) {
    // Optionally read ID (0xAB expected for APDS-9960), but don't fail hard
    (void)read_reg(APDS_ID);
    // Integration time ~100ms: ATIME = 256 - (100/2.78ms) ≈ 256 - 36 = 220 (0xDC)
    write_reg(APDS_ATIME, 0xDC);
    // ALS gain 4x
    write_reg(APDS_CONTROL, 0x01);
    // Power ON and ALS enable
    write_reg(APDS_ENABLE, APDS_ENABLE_PON | APDS_ENABLE_AEN);
    return true;
}

bool apds9960_read_rgbc(uint16_t* r, uint16_t* g, uint16_t* b, uint16_t* c) {
    uint8_t buf[8];
    read_multi(APDS_CDATAL, buf, 8);
    uint16_t cval = (uint16_t)buf[1] << 8 | buf[0];
    uint16_t rval = (uint16_t)buf[3] << 8 | buf[2];
    uint16_t gval = (uint16_t)buf[5] << 8 | buf[4];
    uint16_t bval = (uint16_t)buf[7] << 8 | buf[6];
    if (r) { 
        *r = rval; 
    }
    if (g) { 
        *g = gval; 
    }
    if (b) { 
        *b = bval; 
    }
    if (c) { 
        *c = cval; 
    }
    return true;
}

Color apds9960_classify(uint16_t r, uint16_t g, uint16_t b, uint16_t c) {
    (void)c; // Clear channel not used in this simple classification

    // Ratio-based simple classification (tunable)
    // Avoid division; compare scaled integers
    if (r >= (g + g/2) && r >= (b + b/2)) { return COLOR_RED; } // r >= 1.5*g, r >= 1.5*b approx
    if (g >= (r + r/3) && g >= (b + b/5)) { return COLOR_GREEN; } // g >= 1.33*r, 1.2*b approx
    if (b >= (r + r/3) && b >= (g + g/5)) { return COLOR_BLUE; }  // b >= 1.33*r, 1.2*g approx
    return COLOR_OTHER;
}
