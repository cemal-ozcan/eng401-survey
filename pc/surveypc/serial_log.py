"""Save everything the ESP32 prints on its USB serial port to a text file.

Each line is prefixed with the PC time (Unix ms) and a TAB, so replay tools can
recover timing. Works with the diagnostic firmware (also in raw NMEA mode) and
the survey firmware.

  .venv/bin/python -m surveypc.serial_log --list
  .venv/bin/python -m surveypc.serial_log --port COM5 --out logs/static_test.txt     (Windows)
  .venv/bin/python -m surveypc.serial_log --port /dev/cu.usbserial-0001 --out log.txt (macOS)
"""

from __future__ import annotations

import argparse
import signal
import sys
import time
from pathlib import Path


def split_prefix(line: str) -> tuple[int | None, str]:
    """'1700000000123<TAB>text' -> (1700000000123, 'text'); lines without prefix -> (None, line)."""
    head, sep, rest = line.partition("\t")
    if sep and head.isdigit() and len(head) >= 12:
        return int(head), rest
    return None, line


def main() -> None:
    import serial  # pyserial
    from serial.tools import list_ports

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", type=Path, default=Path(f"serial_{time.strftime('%Y%m%d_%H%M%S')}.txt"))
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list or not args.port:
        for p in list_ports.comports():
            print(f"{p.device:25} {p.description}")
        if not args.port:
            print("\nchoose one with --port")
        return

    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    lines = 0
    with serial.Serial(args.port, args.baud, timeout=0.5) as ser, args.out.open("w", buffering=1) as f:
        print(f"logging {args.port} @ {args.baud} to {args.out} (Ctrl+C to stop)")
        try:
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                f.write(f"{int(time.time() * 1000)}\t{text}\n")
                lines += 1
                print(text)
        except KeyboardInterrupt:
            pass
    print(f"\n{lines} lines saved to {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
