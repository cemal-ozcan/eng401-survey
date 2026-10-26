"""Survey record codec: SD-card CSV and UDP JSON telemetry (docs/ICD.md).

Mirrors firmware/lib/record. Missing values are None (empty CSV field / JSON null).
encode_data_json() reproduces the firmware's byte format exactly, so the virtual
ESP32 sends the same packets as the real device.
"""

from __future__ import annotations

import json
import math
from typing import Any

PROTOCOL_VERSION = 1

# (column, type, decimals) in ICD order. type: "int", "float", "str".
FIELDS: list[tuple[str, str, int]] = [
    ("seq", "int", 0),
    ("t_us", "int", 0),
    ("utc", "str", 0),
    ("fix", "int", 0),
    ("sats", "int", 0),
    ("hdop", "float", 2),
    ("lat", "float", 8),
    ("lon", "float", 8),
    ("alt", "float", 1),
    ("spd", "float", 2),
    ("cog", "float", 1),
    ("gnss_age_ms", "int", 0),
    ("utm_zone", "int", 0),
    ("utm_hemi", "str", 0),
    ("easting", "float", 3),
    ("northing", "float", 3),
    ("ax", "float", 3),
    ("ay", "float", 3),
    ("az", "float", 3),
    ("gx", "float", 2),
    ("gy", "float", 2),
    ("gz", "float", 2),
    ("temp", "float", 2),
    ("pres", "float", 2),
    ("hum", "float", 1),
    ("dist", "int", 0),
    ("flags", "int", 0),
    ("missed", "int", 0),
    ("mark", "int", 0),
]
COLUMNS = [name for name, _, _ in FIELDS]
CSV_HEADER = ",".join(COLUMNS)

# flags bitmask (ICD 2.1)
FLAG_GNSS_FIX = 1 << 0
FLAG_IMU_OK = 1 << 1
FLAG_ENV_OK = 1 << 2
FLAG_DIST_OK = 1 << 3
FLAG_SD_OK = 1 << 4
FLAG_WIFI = 1 << 5
FLAG_UTC_SYNCED = 1 << 6
FLAG_MOCK = 1 << 7

Record = dict[str, Any]


class ProtocolError(ValueError):
    """Raised for malformed CSV rows or packets."""


def _convert(kind: str, text: str) -> Any:
    if text == "":
        return None
    if kind == "int":
        return int(text)
    if kind == "float":
        return float(text)
    return text


def parse_csv_line(line: str) -> Record:
    """Parse one CSV data row (not the header) into a record dict."""
    parts = line.rstrip("\r\n").split(",")
    if len(parts) != len(FIELDS):
        raise ProtocolError(f"expected {len(FIELDS)} columns, got {len(parts)}")
    try:
        return {name: _convert(kind, text) for (name, kind, _), text in zip(FIELDS, parts)}
    except ValueError as exc:
        raise ProtocolError(str(exc)) from exc


def decode_packet(payload: bytes | str) -> Record:
    """Decode one UDP datagram. Returns the JSON object (any type: data, hb, pong)."""
    try:
        obj = json.loads(payload)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise ProtocolError(f"invalid JSON: {exc}") from exc
    if not isinstance(obj, dict) or obj.get("v") != PROTOCOL_VERSION or "type" not in obj:
        raise ProtocolError("not a v1 packet")
    if obj["type"] == "data":
        missing = [c for c in COLUMNS if c not in obj]
        if missing:
            raise ProtocolError(f"data packet missing fields: {missing}")
    return obj


def _is_missing(value: Any) -> bool:
    return value is None or (isinstance(value, float) and math.isnan(value))


def _format_value(kind: str, decimals: int, value: Any, json_mode: bool) -> str:
    if _is_missing(value):
        return "null" if json_mode else ""
    if kind == "float":
        return f"{value:.{decimals}f}"
    if kind == "int":
        return str(int(value))
    return json.dumps(str(value)) if json_mode else str(value)


def encode_csv_line(rec: Record) -> str:
    """Encode a record as a CSV row (no line ending), identical to the firmware."""
    return ",".join(_format_value(k, d, rec.get(n), False) for n, k, d in FIELDS)


def encode_data_json(rec: Record) -> str:
    """Encode a record as a 'data' telemetry packet, identical to the firmware."""
    body = ",".join(f'"{n}":{_format_value(k, d, rec.get(n), True)}' for n, k, d in FIELDS)
    return f'{{"v":{PROTOCOL_VERSION},"type":"data",{body}}}'
