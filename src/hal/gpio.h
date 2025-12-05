/*
 * HAL GPIO: simple pin mode/read/write helpers for ATmega328P pins.
 */
#pragma once
#include <stdint.h>

typedef enum {
    PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7,
    PIN_D8, PIN_D9, PIN_D10, PIN_D11, PIN_D12, PIN_D13,
    PIN_A0, PIN_A1, PIN_A2, PIN_A3, PIN_A4, PIN_A5
} GpioPin;

typedef enum { GPIO_INPUT=0, GPIO_INPUT_PULLUP=1, GPIO_OUTPUT=2 } GpioMode;

typedef enum { GPIO_LOW=0, GPIO_HIGH=1 } GpioLevel;

/** Configure a pin's direction and pull-up mode. */
void gpio_pin_mode(GpioPin pin, GpioMode mode);

/** Drive a digital output pin high or low. */
void gpio_write(GpioPin pin, GpioLevel level);

/** Read the current level of a digital input pin. */
GpioLevel gpio_read(GpioPin pin);
