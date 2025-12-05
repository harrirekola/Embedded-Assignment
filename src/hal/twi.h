/*
 * HAL TWI (I2C): blocking master-mode transactions for VL6180 and
 * APDS9960 sensors.
 */
#pragma once
#include <stdint.h>

/** Initialize TWI hardware for 100kHz master mode. */
void twi_init(void);

/** Send START and slave address (7-bit<<1 | R/W) and wait.
 * @param addr Address byte including R/W bit.
 * @return TWSR status code (upper 5 bits) or 0 on timeout.
 */
uint8_t twi_start(uint8_t addr);

/** Issue STOP condition on the bus. */
void twi_stop(void);

/** Write one data byte and wait for completion.
 * @return TWSR status code (upper 5 bits) or 0 on timeout.
 */
uint8_t twi_write(uint8_t data);

/** Read one byte and acknowledge (more bytes to come).
 * @return Byte value read or 0 on timeout.
 */
uint8_t twi_read_ack(void);

/** Read one byte and NACK (last byte).
 * @return Byte value read or 0 on timeout.
 */
uint8_t twi_read_nack(void);
