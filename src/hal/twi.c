/*
 * HAL TWI (I2C) implementation
 * ----------------------------
 * Blocking master-mode helpers used by the VL6180 and APDS9960 drivers.
 * Simplicity over features: fixed 100 kHz, basic timeout to avoid lockups,
 * and minimal status handling sufficient for bring-up and polling.
 */
#include <avr/io.h>
#include "twi.h"
#include "platform/config.h"

static inline uint8_t twi_wait_twint(void) {
    uint32_t loops = TWI_TIMEOUT_LOOPS;
    while (!(TWCR & (1<<TWINT))) {
        if (--loops == 0) { return 0; }
    }
    return 1;
}

void twi_init(void) {
    TWSR = 0x00; // prescaler 1
    TWBR = (uint8_t)(((F_CPU/TWI_FREQ_HZ)-16)/2);
    TWCR = (1<<TWEN);
}
uint8_t twi_start(uint8_t addr) {
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    if (!twi_wait_twint()) { return 0; }
    TWDR = addr;
    TWCR = (1<<TWINT)|(1<<TWEN);
    if (!twi_wait_twint()) { return 0; }
    return (TWSR & 0xF8);
}
void twi_stop(void) {
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}
uint8_t twi_write(uint8_t data) {
    TWDR = data;
    TWCR = (1<<TWINT)|(1<<TWEN);
    if (!twi_wait_twint()) { return 0; }
    return (TWSR & 0xF8);
}
uint8_t twi_read_ack(void) {
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
    if (!twi_wait_twint()) { return 0; }
    return TWDR;
}
uint8_t twi_read_nack(void) {
    TWCR = (1<<TWINT)|(1<<TWEN);
    if (!twi_wait_twint()) { return 0; }
    return TWDR;
}
