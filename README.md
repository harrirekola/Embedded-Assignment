This is repository for This code is developed by Tampere University for course COMP.SE.200 Software Testing to work as an assignment for students focusing on embedded systems. Use and distribution of this code outside the course context is not allowed.

# Conveyor sorting machine firmware

Embedded firmware for a small conveyor belt sorting system used in lab exercises. It runs on an Arduino Nano with ATmega328P microcontroller and interfaces with:

- TB6600 stepper driver (belt motion)
- 3 hobby servos (diverters)
- VL6180 time‑of‑flight distance sensor (presence)
- APDS9960 color sensor (classification)
- UART logging at 115200 baud

The application senses, classifies, routes, schedules, and actuates diverters so blocks are directed to positions based on color and length.

---

## Quick start

1) Prerequisites
- Windows 10/11 with PowerShell
- AVR toolchain (either via Arduino IDE/Boards Manager, or Microchip avr8‑gnu‑toolchain)
  - The build/flash scripts auto‑discover common install locations under `%LocalAppData%\Arduino15` or `Program Files`.

2) Build
- Run `scripts/build.ps1` from PowerShell. It will compile all sources under `src/` and produce:
  - `build/firmware.elf`
  - `build/firmware.hex`

3) Flash
- Connect the Arduino Nano compatible board.
- Run `scripts/flash.ps1 -Port COM4` (adjust `-Port` as needed). The script uses `avrdude` and defaults to 115200 baud.

4) Observe logs
- Open a serial terminal at 115200 8N1 on the same COM port. You’ll see compact, parseable log lines like DETECT/CLEAR/CLASSIFY/SCHEDULE/ACTUATE/COUNT according to `utils/log.c`.

---

## Project layout

- `src/`
  - `main.c` – boot, init modules, main loop and ticks
  - `app/`
    - `sense.c` – sessions (detect→clear), length computation, APDS sampling and classification
    - `decide.c` – route and schedule future actuations from detection timestamps and belt speed
    - `actuate.c` – servo control orchestration and counters (auto‑center dwell timers)
    - `interrupts.c` – external interrupt wiring for VL6180 GPIO1 → INT0
  - `drivers/`
    - `tb6600.c` – stepper setup and belt speed/rate
    - `servo.c` – Timer2 ISR based 50 Hz software PWM for up to 3 servos
    - `vl6180.c` – ToF init, thresholds, continuous mode, interrupt clear
    - `apds9960.c` – color sensor minimal driver and basic classification helper
  - `hal/`
    - `timers.c` – Timer0 based millis() timebase (1 kHz)
    - `twi.c` – I2C/TWI helpers for sensors
    - `uart.c` – TX‑only UART for logging
    - `gpio.c` – basic GPIO abstraction
  - `platform/`
    - `config.h` – geometry, speeds, thresholds, timing constants
    - `pins.h` – Arduino Nano pin mapping (D‑pins to peripherals)
  - `utils/`
    - `log.c/.h` – compact UART log formatting
- `scripts/`
  - `build.ps1` – one‑shot compile & link; auto‑finds toolchain
  - `flash.ps1` – flash HEX via avrdude; `-Port` can be set (default COM4)
- `build/` – build artifacts (created by build script)

---

## Configuration knobs (from `platform/config.h`)

- Geometry and kinematics
  - `STEPPER_FULL_STEPS_PER_REV`, `TB6600_MICROSTEPS`
  - `GEAR_PINION_TEETH`, `GEAR_PULLEY_TEETH`, `ROLLER_DIAMETER_MM`
  - Derived: `MM_PER_PULSE_X1000`
- Default runtime parameters
  - `BELT_MM_PER_S` (fallback), `SERVO_D1_MM/D2_MM/D3_MM`, `SERVO_DWELL_MS`
  - `VL6180_MEAS_PERIOD_MS`, `VL6180_QUIET_TIMEOUT_MS`
  - `LENGTH_SMALL_MAX_MM`, `ACTUATION_ADVANCE_MS`
  - `DECIDE_MIN_SPACING_MS`, `DECIDE_MAX_BLOCKS_PER_MIN`
- I/O
  - `UART_BAUD` (115200)
  - `TWI_FREQ_HZ` (100 kHz), `TWI_TIMEOUT_LOOPS`

These can be tuned to your hardware. Belt speed is typically set at runtime from the stepper rate.

---

## Build details

The build script uses:
- MCU: `atmega328p`
- Flags: `-Os -Wall -Wextra -Werror -ffunction-sections -fdata-sections`
- Linker: `--gc-sections`
- Includes: `src/` and subfolders

If the toolchain isn’t found, install Arduino AVR Boards (via Arduino IDE) or Microchip avr8‑gnu‑toolchain and re‑run the script.

---

## Flash details

`flash.ps1` wraps `avrdude`.
- Default programmer: `arduino`, MCU: `m328p`
- Example: `scripts/flash.ps1 -Port COM5` or try auto baud fallback with `-Baud 0`.

---

## Logging format

Logs are compact, line‑oriented ASCII per `utils/log.c`, e.g.:
- `DETECT t=... id=...`
- `CLEAR t=... id=...`
- `CLASSIFY t=... id=... color=R/G/B/Other len_mm=... class=Small/NotSmall thr=...`
- `SCHEDULE t=... id=... pos=Pos1/Pos2/Pos3/PassThrough at=...`
- `ACTUATE t=... id=... pos=...`
- `COUNT t=... total=... diverted=... passed=... fault=... red=... green=... blue=... other=...`

Use a serial terminal or capture logs for offline parsing.


