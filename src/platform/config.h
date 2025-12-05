/*
 * Platform config: compile-time defaults for belt speed, thresholds,
 * and distances. Tune during calibration.
 */
#pragma once
// Config defaults (to be tuned during calibration)
// Mechanical configuration for belt kinematics
// Stepper/TB6600
#define STEPPER_FULL_STEPS_PER_REV 200UL
#define TB6600_MICROSTEPS           8UL     // microstep setting => pulses/rev = 1600

// Belt transmission
#define GEAR_PINION_TEETH          20UL     // motor pinion
#define GEAR_PULLEY_TEETH          50UL     // driven pulley on roller
#define ROLLER_DIAMETER_MM         40UL     // roller diameter

// Fixed-point PI scaled by 1000 to avoid float
#define PI_1000                  3142UL

// Derived geometry (all integer math, scaled by 1000 where noted)
#define PULSES_PER_REV  (STEPPER_FULL_STEPS_PER_REV * TB6600_MICROSTEPS)        // 1600
#define MM_PER_MOTOR_REV_X1000   (PI_1000 * ROLLER_DIAMETER_MM)                 // ~125660
#define MM_PER_ROLLER_REV_X1000  ( (MM_PER_MOTOR_REV_X1000 * GEAR_PINION_TEETH) / GEAR_PULLEY_TEETH ) // ~50264
#define MM_PER_PULSE_X1000       (MM_PER_ROLLER_REV_X1000 / PULSES_PER_REV)    // ~31

// Default belt speed fallback (mm/s); runtime will compute from step rate and geometry
#define BELT_MM_PER_S 55
#define TOF_THRESHOLD_MM 60
#define TOF_HYST_MM 5

// Approximate distances from ToF to actuators (mm)
#define SERVO_D1_MM 120   // 12 cm
#define SERVO_D2_MM 240   // 24 cm
#define SERVO_D3_MM 360   // 36 cm
#define UART_BAUD 115200
#define DEBOUNCE_MS 10

// How long to hold servo at deflect position before auto-centering (ms)
#define SERVO_DWELL_MS 250

// Startup mute period for servos (ms): keep outputs low to avoid jitter, then start centered pulses
#define SERVO_STARTUP_MUTE_MS 1500

// Minimum interval between COUNT logs when no changes (ms)
#define COUNT_LOG_MIN_INTERVAL_MS 10000

// VL6180 single-shot measurement cadence and session quiet timeout (ms)
#define VL6180_MEAS_PERIOD_MS 50
#define VL6180_QUIET_TIMEOUT_MS 400

// I2C/TWI bus configuration
#define TWI_FREQ_HZ 100000UL
#define TWI_TIMEOUT_LOOPS 50000UL

// Length classification threshold (mm): smaller than this is LEN_SMALL
#define LENGTH_SMALL_MAX_MM 50

// Advance actuation to occur earlier than nominal time-of-flight arrival.
// Positive value subtracts from scheduled due time (ms), clamped to detection time.
#define ACTUATION_ADVANCE_MS 500

// Scheduler capacity: max number of pending actuations queued
#define SCHED_CAPACITY 4

// Decide module defaults (used by main to initialize runtime settings)
#define DECIDE_MIN_SPACING_MS 1000
#define DECIDE_MAX_BLOCKS_PER_MIN 15
