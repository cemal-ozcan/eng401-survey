"""Turn logs from the hardware team into survey CSV files (docs/ICD.md format),
so the analysis and dashboard pipeline can be tested with real data.

Input (detected automatically, with or without serial_log time prefixes):
  - raw NMEA (diag firmware 'n' mode, u-center, any GNSS logger)
      -> one record per GGA epoch; UTM via pyproj; sensor columns empty
  - serial output of the survey firmware ('#' lines + CSV records)
      -> the CSV records, cleaned

  .venv/bin/python -m surveypc.replay static_test.txt --out data/raw/static_test.csv
  .venv/bin/python -m surveypc.replay static_test.txt --out x.csv --send     (also stream as live telemetry)
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from . import nmea
from .protocol import (CSV_HEADER, FIELDS, FLAG_GNSS_FIX, FLAG_UTC_SYNCED, ProtocolError, Record,
                       encode_csv_line, parse_csv_line)
from .serial_log import split_prefix
from .utm_check import reference_utm


def utm_zone(lat: float, lon: float) -> int:
    """Standard UTM zone incl. Norway / Svalbard exceptions (same rules as the firmware)."""
    zone = min(int(math.floor((lon + 180.0) / 6.0)) + 1, 60)
    if 56.0 <= lat < 64.0 and 3.0 <= lon < 12.0:
        zone = 32
    if lat >= 72.0:
        for lo, hi, z in ((0, 9, 31), (9, 21, 33), (21, 33, 35), (33, 42, 37)):
            if lo <= lon < hi:
                zone = z
    return zone


def _empty_record() -> Record:
    return {name: None for name, _, _ in FIELDS}


def nmea_to_records(lines: list[str]) -> tuple[list[Record], dict]:
    parser = nmea.Parser()
    records: list[Record] = []
    utc0 = None
    for raw in lines:
        _, line = split_prefix(raw)
        line = line.strip()
        if not line.startswith("$"):
            continue
        if parser.process(line) != "GGA":
            continue
        d = parser.data
        r = _empty_record()
        r["seq"] = len(records)
        utc = d.utc
        if utc is not None and utc0 is None:
            utc0 = utc
        r["t_us"] = int((utc - utc0).total_seconds() * 1e6) if utc is not None else r["seq"] * 1_000_000
        r["utc"] = utc.strftime("%Y-%m-%dT%H:%M:%S.") + f"{utc.microsecond // 1000:03d}Z" if utc else None
        r["fix"], r["sats"] = d.fix_quality, d.satellites
        r["hdop"] = None if math.isnan(d.hdop) else d.hdop
        flags = FLAG_UTC_SYNCED if utc else 0
        if d.pos_valid:
            zone = utm_zone(d.lat, d.lon)
            hemi = "N" if d.lat >= 0 else "S"
            e, n = reference_utm(d.lat, d.lon, zone, hemi)
            r.update(lat=d.lat, lon=d.lon, alt=None if math.isnan(d.alt) else d.alt,
                     spd=None if math.isnan(d.speed_kmh) else d.speed_kmh,
                     cog=None if math.isnan(d.course_deg) else d.course_deg,
                     gnss_age_ms=0, utm_zone=zone, utm_hemi=hemi, easting=e, northing=n)
            flags |= FLAG_GNSS_FIX
        r["dist"], r["flags"], r["missed"], r["mark"] = 0, flags, 0, 0
        records.append(r)
    return records, {"sentences": parser.sentences, "checksum_errors": parser.checksum_errors}


def serial_to_records(lines: list[str]) -> tuple[list[Record], dict]:
    records, skipped = [], 0
    for raw in lines:
        _, line = split_prefix(raw)
        line = line.strip()
        if not line or line.startswith("#") or line == CSV_HEADER:
            continue
        try:
            records.append(parse_csv_line(line))
        except ProtocolError:
            skipped += 1  # boot messages, partial lines
    return records, {"skipped_lines": skipped}


def detect(lines: list[str]) -> str:
    nmea_count = csv_count = 0
    for raw in lines[:2000]:
        _, line = split_prefix(raw)
        if line.startswith("$") and "*" in line:
            nmea_count += 1
        elif line.count(",") == len(FIELDS) - 1:
            csv_count += 1
    return "nmea" if nmea_count > csv_count else "serial"


def convert(path: Path) -> tuple[list[Record], str, dict]:
    lines = Path(path).read_text(errors="replace").splitlines()
    kind = detect(lines)
    records, info = nmea_to_records(lines) if kind == "nmea" else serial_to_records(lines)
    return records, kind, info


def write_survey(records: list[Record], out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(CSV_HEADER + "\n" + "".join(encode_csv_line(r) + "\n" for r in records))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--send", action="store_true", help="afterwards stream it as live telemetry (virtual ESP32)")
    ap.add_argument("--rate", type=float, default=1.0)
    ap.add_argument("--port", type=int, default=5005)
    args = ap.parse_args()
    records, kind, info = convert(args.input)
    write_survey(records, args.out)
    fixes = sum(1 for r in records if r["flags"] & FLAG_GNSS_FIX)
    print(f"{args.input.name}: detected {kind}, {len(records)} records ({fixes} with fix), {info} -> {args.out}")
    if args.send and records:
        import time

        from .virtual_esp32 import VirtualEsp32

        with VirtualEsp32(args.out, pc_port=args.port, rate_hz=args.rate, loop=False) as dev:
            print("streaming... Ctrl+C to stop")
            try:
                while dev.sent + dev.dropped < len(records):
                    time.sleep(0.5)
            except KeyboardInterrupt:
                pass


if __name__ == "__main__":
    main()
