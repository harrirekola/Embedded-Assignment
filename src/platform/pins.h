/*
 * Platform pins: Arduino Nano (ATmega328P) pin assignments for sensors,
 * stepper driver, and servos. Keep in sync with docs/pinout.md.
 */
#pragma once
// Pin mapping placeholder - update to match docs/pinout.md
#define PIN_VL6180_INT 2

// Illumination LEDs for color detection (assumed D3 and D4).
#define PIN_LED_A 3
#define PIN_LED_B 4
#define PIN_TB6600_STEP 9
#define PIN_TB6600_DIR 8
#define PIN_TB6600_EN 7
#define PIN_SERVO1 5
#define PIN_SERVO2 6
#define PIN_SERVO3 10

// HAL GPIO mapping helpers
#include "hal/gpio.h"
// Map Arduino D-pin number N (0..13) to HAL GpioPin enum
#define GPIO_PIN_D(N) ((GpioPin)(PIN_D0 + (N)))

// Concrete HAL pins for peripherals
#define GPIO_PIN_TB6600_STEP GPIO_PIN_D(PIN_TB6600_STEP)
#define GPIO_PIN_TB6600_DIR  GPIO_PIN_D(PIN_TB6600_DIR)
#define GPIO_PIN_TB6600_EN   GPIO_PIN_D(PIN_TB6600_EN)

#define GPIO_PIN_SERVO1 GPIO_PIN_D(PIN_SERVO1)
#define GPIO_PIN_SERVO2 GPIO_PIN_D(PIN_SERVO2)
#define GPIO_PIN_SERVO3 GPIO_PIN_D(PIN_SERVO3)

// Onboard LED (Arduino Nano D13) reserved for ToF presence indication
#define PIN_ONBOARD_LED 13
#define GPIO_PIN_ONBOARD_LED GPIO_PIN_D(PIN_ONBOARD_LED)
#define GPIO_PIN_PRESENCE_LED GPIO_PIN_ONBOARD_LED

// Convenience: HAL pins for illumination LEDs and VL6180 INT
#define GPIO_PIN_LED_A GPIO_PIN_D(PIN_LED_A)
#define GPIO_PIN_LED_B GPIO_PIN_D(PIN_LED_B)
#define GPIO_PIN_VL6180_INT GPIO_PIN_D(PIN_VL6180_INT)
