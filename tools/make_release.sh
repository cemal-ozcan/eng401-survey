#!/usr/bin/env bash
# Builds a PlatformIO env and merges bootloader + partitions + app into ONE .bin
# that is flashed at offset 0x0 (browser flasher / esptool), so the hardware team
# never needs PlatformIO.
#
# Usage: tools/make_release.sh <env> <release-name>     e.g. tools/make_release.sh diag v0.1-diag
set -euo pipefail

ENV="${1:?env name required (diag, esp32, ...)}"
NAME="${2:?release name required (e.g. v0.1-diag)}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PIO="$ROOT/.venv/bin/pio"
PY="$ROOT/.venv/bin/python"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
BUILD="$ROOT/firmware/.pio/build/$ENV"
OUT_DIR="$ROOT/dist/$NAME"
OUT="$OUT_DIR/eng401-$NAME.bin"

(cd "$ROOT/firmware" && "$PIO" run -e "$ENV")
mkdir -p "$OUT_DIR"
"$PY" "$ESPTOOL" --chip esp32 merge_bin -o "$OUT" \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 "$BUILD/bootloader.bin" \
  0x8000 "$BUILD/partitions.bin" \
  0xe000 "$BOOT_APP0" \
  0x10000 "$BUILD/firmware.bin"

shasum -a 256 "$OUT" | tee "$OUT.sha256"
echo "Release ready: $OUT"
