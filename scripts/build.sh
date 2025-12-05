#!/usr/bin/env bash
set -euo pipefail

# Build firmware for ATmega328P (Arduino Nano) on macOS/Linux.
# Robust to spaces in project paths; avoids broken sources.list usage.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SRC_DIR="${PROJECT_ROOT}/src"
BUILD_DIR="${PROJECT_ROOT}/build"

mkdir -p "${BUILD_DIR}"

# Clean up any stale sources.list from older scripts
[[ -f "${BUILD_DIR}/sources.list" ]] && rm -f "${BUILD_DIR}/sources.list"

# Gather sources (exclude removed modules) — compatible with macOS Bash 3.x (no mapfile)
SOURCES=()
while IFS= read -r -d '' f; do
  SOURCES+=("$f")
done < <(find "${SRC_DIR}" -type f -name '*.c' \
  ! -name 'commands.c' ! -name 'calibration.c' -print0 | sort -z)
if [[ ${#SOURCES[@]} -eq 0 ]]; then
  echo "[build] No C sources found under ${SRC_DIR}" >&2
  exit 1
fi

ELF="${BUILD_DIR}/firmware.elf"
HEX="${BUILD_DIR}/firmware.hex"

find_tool() {
  local name="$1"
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  local bases=(
    "$HOME/Library/Arduino15/packages/arduino/tools/avr-gcc"
    "/Applications/Arduino.app/Contents/Java/hardware/tools/avr/bin"
    "/Applications/Arduino.app/Contents/Resources/app" # Arduino IDE 2.x resources (fallback)
  )
  local exe
  for base in "${bases[@]}"; do
    [[ -d "$base" ]] || continue
    exe="$(/usr/bin/find "$base" -maxdepth 6 -type f -name "$name" -perm +111 2>/dev/null | head -n1)"
    if [[ -n "${exe}" ]]; then
      echo "$exe"
      return 0
    fi
    exe="$(/usr/bin/find "$base" -maxdepth 6 -type f -name "$name.exe" 2>/dev/null | head -n1)"
    if [[ -n "${exe}" ]]; then
      echo "$exe"
      return 0
    fi
  done
  return 1
}

GCC="$(find_tool avr-gcc || true)"
OBJCOPY="$(find_tool avr-objcopy || true)"
SIZE="$(find_tool avr-size || true)"

if [[ -z "${GCC}" ]]; then
  echo "[build] ERROR: avr-gcc not found. Install Arduino IDE (AVR Boards) or avr-gcc via Homebrew (osx-cross/avr/avr-gcc), or add to PATH." >&2
  exit 1
fi
if [[ -z "${OBJCOPY}" ]]; then
  echo "[build] ERROR: avr-objcopy not found. Install Arduino IDE (AVR Boards) or add toolchain to PATH." >&2
  exit 1
fi

echo "[build] avr-gcc: ${GCC}"
[[ -n "${SIZE}" ]] && echo "[build] avr-size: ${SIZE}" || echo "[build] avr-size not found; size report skipped."
echo "[build] avr-objcopy: ${OBJCOPY}"
echo "[build] Compiling ${#SOURCES[@]} sources..."

DEFINES=(
  -DF_CPU=16000000UL
)
CFLAGS=(
  -mmcu=atmega328p
  -Os
  -Wall -Wextra -Werror
  "${DEFINES[@]}"
  -ffunction-sections -fdata-sections
)
LFLAGS=(
  -Wl,--gc-sections
)
INCLUDES=(
  -I"${SRC_DIR}"
  -I"${SRC_DIR}/hal"
  -I"${SRC_DIR}/drivers"
  -I"${SRC_DIR}/app"
  -I"${SRC_DIR}/utils"
  -I"${SRC_DIR}/platform"
)

# Invoke compiler with proper array expansion (handles spaces correctly)
"${GCC}" "${CFLAGS[@]}" "${INCLUDES[@]}" "${SOURCES[@]}" "${LFLAGS[@]}" -o "${ELF}"

# Convert to Intel HEX
"${OBJCOPY}" -O ihex -R .eeprom "${ELF}" "${HEX}"

# Optional size report
if [[ -n "${SIZE}" ]]; then
  "${SIZE}" -C --mcu=atmega328p "${ELF}"
fi

echo "[build] Output: ${ELF}"
echo "[build] HEX: ${HEX}"
