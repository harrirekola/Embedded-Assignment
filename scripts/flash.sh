#!/usr/bin/env bash
set -euo pipefail

# Flash firmware to ATmega328P using avrdude on macOS/Linux.
# Defaults are suitable for Arduino Nano compatible bootloaders.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
HEX_REL="build/firmware.hex"
HEX="${PROJECT_ROOT}/${HEX_REL}"

PORT=""
BAUD="115200"   # set to 0 for auto-try 115200 then 57600
PROGRAMMER="arduino"
MCU="m328p"

usage() {
  cat <<EOF
Usage: $0 [-p <port>] [-b <baud|0>] [-P <programmer>] [-m <mcu>]
  -p <port>        Serial port (e.g., /dev/tty.usbserial-XXXX). If omitted, attempts auto-detect.
  -b <baud|0>      Baud rate. 115200 (default) or 57600. Use 0 to auto-try 115200 then 57600.
  -P <programmer>  avrdude programmer ID (default: arduino)
  -m <mcu>         MCU ID (default: m328p)
EOF
}

while getopts ":p:b:P:m:h" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
    P) PROGRAMMER="$OPTARG" ;;
    m) MCU="$OPTARG" ;;
    h) usage; exit 0 ;;
    :) echo "Missing argument for -$OPTARG" >&2; usage; exit 1 ;;
    \?) echo "Unknown option: -$OPTARG" >&2; usage; exit 1 ;;
  esac
done

if [[ ! -f "${HEX}" ]]; then
  echo "[flash] ERROR: HEX not found at '${HEX}'. Build first (scripts/build.sh)." >&2
  exit 1
fi

find_tool() {
  local name="$1"
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  local bases=(
    "$HOME/Library/Arduino15/packages/arduino/tools/avrdude"
    "/Applications/Arduino.app/Contents/Java/hardware/tools/avr/bin"
    "/Applications/Arduino.app/Contents/Resources/app" # Arduino IDE 2.x resources (fallback)
  )
  local exe
  for base in "${bases[@]}"; do
    [[ -d "$base" ]] || continue
    exe="$(/usr/bin/find "$base" -maxdepth 6 -type f -name avrdude -perm +111 2>/dev/null | head -n1)"
    if [[ -n "${exe}" ]]; then
      echo "$exe"
      return 0
    fi
    exe="$(/usr/bin/find "$base" -maxdepth 6 -type f -name avrdude.exe 2>/dev/null | head -n1)"
    if [[ -n "${exe}" ]]; then
      echo "$exe"
      return 0
    fi
  done
  return 1
}

find_avrdude_conf() {
  local exe="$1"
  local dir="$(dirname "$exe")"
  # Common locations relative to avrdude binary
  local candidates=(
    "$(dirname "$dir")/etc/avrdude.conf"          # .../tools/avrdude/<ver>/etc
    "$(dirname "$(dirname "$dir")")/etc/avrdude.conf"  # .../tools/avrdude/etc
    "$(dirname "$dir")/../etc/avrdude.conf"       # .../avr/bin -> .../avr/etc
  )
  for c in "${candidates[@]}"; do
    [[ -f "$c" ]] && { echo "$c"; return 0; }
  done
  # Last resort: search within Arduino15/tools/avrdude
  local conf
  conf="$(/usr/bin/find "$HOME/Library/Arduino15/packages/arduino/tools/avrdude" -maxdepth 6 -type f -name avrdude.conf 2>/dev/null | head -n1 || true)"
  [[ -n "$conf" ]] && { echo "$conf"; return 0; }
  return 1
}

AVRDUDE="$(find_tool avrdude || true)"
if [[ -z "${AVRDUDE}" ]]; then
  echo "[flash] ERROR: avrdude not found. Install Arduino IDE (AVR Boards) or avrdude and add to PATH." >&2
  exit 1
fi

CONF="$(find_avrdude_conf "${AVRDUDE}" || true)"
[[ -n "${CONF}" ]] || echo "[flash] WARNING: Could not locate avrdude.conf automatically; relying on defaults."

autodetect_port() {
  # Prefer usbserial, then usbmodem
  local cand
  cand="$(ls /dev/tty.usbserial* 2>/dev/null | head -n1 || true)"
  [[ -z "$cand" ]] && cand="$(ls /dev/tty.usbmodem* 2>/dev/null | head -n1 || true)"
  echo "$cand"
}

if [[ -z "${PORT}" ]]; then
  PORT="$(autodetect_port || true)"
  if [[ -z "${PORT}" ]]; then
    echo "[flash] ERROR: No port provided and auto-detect failed. Use -p /dev/tty.usbserial-XXXX" >&2
    exit 1
  fi
fi

pushd "${PROJECT_ROOT}" >/dev/null

echo "[flash] avrdude: ${AVRDUDE}"
echo "[flash] Port=${PORT} Programmer=${PROGRAMMER} MCU=${MCU}"
echo "[flash] Image=${HEX_REL}"

invoke_flash() {
  local rate="$1"
  local args=()
  if [[ -n "${CONF:-}" ]]; then args+=( "-C" "${CONF}" ); fi
  args+=( "-p" "${MCU}" "-c" "${PROGRAMMER}" "-P" "${PORT}" "-b" "${rate}" "-D" "-U" "flash:w:${HEX_REL}:i" )
  echo "[flash] Trying baud ${rate} ..."
  "${AVRDUDE}" "${args[@]}"
}

if [[ "${BAUD}" == "0" ]]; then
  if ! invoke_flash 115200; then
    echo "[flash] 115200 failed; falling back to 57600"
    invoke_flash 57600
  fi
else
  invoke_flash "${BAUD}"
fi

echo "[flash] Done."

popd >/dev/null
