"""NMEA 0183 parser for replaying raw GNSS logs on the PC.

Same behaviour as firmware/lib/nmea (GGA / RMC / VTG, any talker), so raw logs
recorded by the hardware team can be pushed through the PC pipeline.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from functools import reduce

KNOTS_TO_KMH = 1.852


def checksum_ok(sentence: str) -> bool:
    s = sentence.strip()
    if not s.startswith("$") or "*" not in s:
        return False
    body, _, cs = s[1:].partition("*")
    if len(cs) != 2:
        return False
    try:
        return reduce(lambda a, c: a ^ ord(c), body, 0) == int(cs, 16)
    except ValueError:
        return False


def parse_coordinate(value: str, hemi: str) -> float:
    """'4807.038','N' -> 48.1173. NaN when empty or malformed."""
    if not value or not hemi:
        return math.nan
    int_part = value.split(".")[0]
    if not (3 <= len(int_part) <= 5) or not int_part.isdigit():
        return math.nan
    raw = float(value)
    degrees = math.floor(raw / 100.0)
    minutes = raw - degrees * 100.0
    if minutes >= 60.0:
        return math.nan
    deg = degrees + minutes / 60.0
    if hemi in ("N", "E"):
        return deg
    if hemi in ("S", "W"):
        return -deg
    return math.nan


def parse_time_of_day(value: str) -> int | None:
    """'123519.50' -> milliseconds since midnight."""
    if len(value) < 6 or not value[:6].isdigit():
        return None
    h, m, s = int(value[0:2]), int(value[2:4]), int(value[4:6])
    if h > 23 or m > 59 or s > 60:
        return None
    frac = 0
    if len(value) > 7 and value[6] == ".":
        digits = value[7:10]
        if not digits.isdigit():
            return None
        frac = int(digits.ljust(3, "0"))
    return ((h * 60 + m) * 60 + s) * 1000 + frac


def parse_date(value: str) -> datetime | None:
    """'021126' (ddmmyy) -> midnight UTC of that date."""
    if len(value) != 6 or not value.isdigit():
        return None
    d, m, y = int(value[0:2]), int(value[2:4]), int(value[4:6])
    year = 1900 + y if y >= 80 else 2000 + y
    try:
        return datetime(year, m, d, tzinfo=timezone.utc)
    except ValueError:
        return None


@dataclass
class GnssData:
    pos_valid: bool = False
    lat: float = math.nan
    lon: float = math.nan
    alt: float = math.nan
    fix_quality: int = 0
    satellites: int = 0
    hdop: float = math.nan
    speed_kmh: float = math.nan
    course_deg: float = math.nan
    time_of_day_ms: int | None = None
    date: datetime | None = None

    @property
    def utc(self) -> datetime | None:
        if self.time_of_day_ms is None or self.date is None:
            return None
        return self.date + timedelta(milliseconds=self.time_of_day_ms)


def _float(text: str) -> float:
    return float(text) if text else math.nan


@dataclass
class Parser:
    data: GnssData = field(default_factory=GnssData)
    sentences: int = 0
    checksum_errors: int = 0

    def process(self, sentence: str) -> str | None:
        """Decode one sentence. Returns 'GGA'/'RMC'/'VTG'/'OTHER', or None if invalid."""
        sentence = sentence.strip()
        if not checksum_ok(sentence):
            self.checksum_errors += 1
            return None
        self.sentences += 1
        fields = sentence[1:].split("*")[0].split(",")
        kind = fields[0][2:5] if len(fields[0]) >= 5 else ""
        handler = {"GGA": self._gga, "RMC": self._rmc, "VTG": self._vtg}.get(kind)
        if handler and handler(fields):
            return kind
        return "OTHER"

    def _set_time(self, text: str) -> None:
        tod = parse_time_of_day(text)
        if tod is not None:
            self.data.time_of_day_ms = tod

    def _gga(self, f: list[str]) -> bool:
        if len(f) < 10:
            return False
        d = self.data
        self._set_time(f[1])
        d.fix_quality = int(f[6]) if f[6] else 0
        d.satellites = int(f[7]) if f[7] else 0
        d.hdop = _float(f[8])
        lat, lon = parse_coordinate(f[2], f[3]), parse_coordinate(f[4], f[5])
        if d.fix_quality > 0 and not (math.isnan(lat) or math.isnan(lon)):
            d.lat, d.lon, d.alt, d.pos_valid = lat, lon, _float(f[9]), True
        else:
            d.pos_valid = False
        return True

    def _rmc(self, f: list[str]) -> bool:
        if len(f) < 10:
            return False
        d = self.data
        self._set_time(f[1])
        date = parse_date(f[9])
        if date is not None:
            d.date = date
        if f[2] != "A":
            d.pos_valid, d.fix_quality = False, 0
            return True
        lat, lon = parse_coordinate(f[3], f[4]), parse_coordinate(f[5], f[6])
        if not (math.isnan(lat) or math.isnan(lon)):
            d.lat, d.lon, d.pos_valid = lat, lon, True
            if d.fix_quality == 0:
                d.fix_quality = 1
        knots = _float(f[7])
        d.speed_kmh = knots * KNOTS_TO_KMH if not math.isnan(knots) else math.nan
        d.course_deg = _float(f[8])
        return True

    def _vtg(self, f: list[str]) -> bool:
        if len(f) < 8:
            return False
        kmh, cog = _float(f[7]), _float(f[1])
        if not math.isnan(kmh):
            self.data.speed_kmh = kmh
        if not math.isnan(cog):
            self.data.course_deg = cog
        return True
