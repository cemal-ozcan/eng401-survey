"""Dashboard view model: turns receiver snapshots and records into display text,
status levels and plot series. No Qt here, so it is unit-tested."""

from __future__ import annotations

import math
from collections import deque
from dataclasses import dataclass, field

from . import protocol as p

OK, WARN, BAD, NONE = "ok", "warn", "bad", "none"

FIX_NAMES = {0: "No fix", 1: "GPS fix", 2: "DGPS", 4: "RTK fixed", 5: "RTK float", 6: "Dead reckoning"}

SENSOR_FLAGS = [
    ("GNSS fix", p.FLAG_GNSS_FIX),
    ("IMU", p.FLAG_IMU_OK),
    ("BME280", p.FLAG_ENV_OK),
    ("Distance", p.FLAG_DIST_OK),
    ("SD card", p.FLAG_SD_OK),
    ("Wi-Fi", p.FLAG_WIFI),
    ("UTC sync", p.FLAG_UTC_SYNCED),
]


@dataclass(frozen=True)
class Field:
    text: str
    level: str = NONE


def fmt(value, decimals: int, unit: str = "") -> str:
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return "—"
    return f"{value:.{decimals}f}{(' ' + unit) if unit else ''}"


def hdop_level(hdop: float | None) -> str:
    """Rule-of-thumb HDOP classes: <=2 good, <=5 moderate, worse = poor."""
    if hdop is None:
        return NONE
    return OK if hdop <= 2.0 else WARN if hdop <= 5.0 else BAD


def connection_field(snap: dict) -> Field:
    age = snap.get("seconds_since_rx")
    if age is None:
        return Field("Waiting for device…", NONE)
    if age < 3.0:
        return Field(f"Connected {snap['device'][0]} ({age:.1f} s)", OK)
    return Field(f"No data for {age:.0f} s", BAD)


def completeness_field(snap: dict) -> Field:
    pct = snap.get("completeness_pct", 0.0)
    if not snap.get("received"):
        return Field("—")
    level = OK if pct >= 99.0 else WARN if pct >= 95.0 else BAD
    return Field(f"{pct:.1f} %  (rx {snap['received']}, lost {snap['lost']})", level)


def latency_field(snap: dict) -> Field:
    lat = snap.get("latency_ms")
    if not lat:
        return Field("waiting for clock sync")
    unc = snap.get("sync_uncertainty_ms") or 0.0
    level = OK if lat["mean"] < 200 else WARN if lat["mean"] < 1000 else BAD
    return Field(f"{lat['last']:.0f} ms (min {lat['min']:.0f} / mean {lat['mean']:.0f} / max {lat['max']:.0f}) "
                 f"±{unc:.1f}", level)


def gnss_fields(rec: dict | None) -> dict[str, Field]:
    if rec is None:
        return {k: Field("—") for k in ("fix", "sats", "hdop", "position", "utm", "alt", "speed", "utc")}
    fix = rec.get("fix") or 0
    has_fix = bool((rec.get("flags") or 0) & p.FLAG_GNSS_FIX)
    sats = rec.get("sats")
    utm = "—"
    if rec.get("utm_zone") is not None:
        utm = f"{rec['utm_zone']}{rec['utm_hemi']}  E {rec['easting']:.2f}  N {rec['northing']:.2f}"
    return {
        "fix": Field(FIX_NAMES.get(fix, f"quality {fix}"), OK if has_fix else BAD),
        "sats": Field(str(sats) if sats is not None else "—",
                      NONE if sats is None else OK if sats >= 6 else WARN if sats >= 4 else BAD),
        "hdop": Field(fmt(rec.get("hdop"), 2), hdop_level(rec.get("hdop"))),
        "position": Field(f"{rec['lat']:.7f}, {rec['lon']:.7f}" if has_fix else "—"),
        "utm": Field(utm),
        "alt": Field(fmt(rec.get("alt"), 1, "m")),
        "speed": Field(fmt(rec.get("spd"), 1, "km/h")),
        "utc": Field(rec.get("utc") or "not synced", OK if rec.get("utc") else WARN),
    }


def sensor_fields(rec: dict | None) -> dict[str, Field]:
    if rec is None:
        return {k: Field("—") for k in ("temp", "pres", "hum", "dist", "accel", "gyro")}
    flags = rec.get("flags") or 0
    acc = [rec.get(k) for k in ("ax", "ay", "az")]
    gyr = [rec.get(k) for k in ("gx", "gy", "gz")]
    return {
        "temp": Field(fmt(rec.get("temp"), 2, "°C"), OK if flags & p.FLAG_ENV_OK else BAD),
        "pres": Field(fmt(rec.get("pres"), 2, "hPa"), OK if flags & p.FLAG_ENV_OK else BAD),
        "hum": Field(fmt(rec.get("hum"), 1, "%RH"), OK if flags & p.FLAG_ENV_OK else BAD),
        "dist": Field(f"{rec['dist']} mm" if flags & p.FLAG_DIST_OK else "no target",
                      OK if flags & p.FLAG_DIST_OK else WARN),
        "accel": Field(" ".join(fmt(v, 2) for v in acc) + " m/s²", OK if flags & p.FLAG_IMU_OK else BAD),
        "gyro": Field(" ".join(fmt(v, 1) for v in gyr) + " °/s", OK if flags & p.FLAG_IMU_OK else BAD),
    }


def flag_fields(rec: dict | None) -> list[tuple[str, Field]]:
    flags = 0 if rec is None else (rec.get("flags") or 0)
    out = [(name, Field("OK" if flags & bit else "—", OK if flags & bit else BAD if rec else NONE))
           for name, bit in SENSOR_FLAGS]
    if rec is not None and flags & p.FLAG_MOCK:
        out.append(("Mode", Field("MOCK sensors", WARN)))
    return out


NO_FIX_HDOP = 99.0

SERIES = ("temp", "pres", "hum", "dist", "az", "hdop", "sats", "latency")


@dataclass
class DashboardModel:
    """Rolling plot series and the survey track."""

    history: int = 600
    t: deque = field(init=False)
    series: dict[str, deque] = field(init=False)
    track_e: list[float] = field(default_factory=list)
    track_n: list[float] = field(default_factory=list)
    marks: list[tuple[int, float, float]] = field(default_factory=list)
    origin: tuple[float, float] | None = None
    zone: str | None = None
    _t0_us: int | None = None

    def __post_init__(self) -> None:
        self.t = deque(maxlen=self.history)
        self.series = {k: deque(maxlen=self.history) for k in SERIES}

    def add(self, rec: dict, latency_ms: float | None = None) -> None:
        if self._t0_us is None or rec["t_us"] < self._t0_us:  # first record or device reboot
            self._t0_us = rec["t_us"]
        self.t.append((rec["t_us"] - self._t0_us) / 1e6)
        flags = rec.get("flags") or 0
        for k in ("temp", "pres", "hum", "az", "hdop", "sats"):
            v = rec.get(k)
            self.series[k].append(math.nan if v is None else float(v))
        if self.series["hdop"][-1] >= NO_FIX_HDOP:  # receivers report 99.99 = "no value"
            self.series["hdop"][-1] = math.nan
        self.series["dist"].append(float(rec["dist"]) if flags & p.FLAG_DIST_OK else math.nan)
        self.series["latency"].append(math.nan if latency_ms is None else latency_ms)

        if flags & p.FLAG_GNSS_FIX and rec.get("easting") is not None:
            zone = f"{rec['utm_zone']}{rec['utm_hemi']}"
            if self.origin is None or zone != self.zone:  # new zone: restart the track
                self.origin = (math.floor(rec["easting"] / 100) * 100, math.floor(rec["northing"] / 100) * 100)
                self.zone = zone
                self.track_e.clear()
                self.track_n.clear()
            e, n = rec["easting"] - self.origin[0], rec["northing"] - self.origin[1]
            self.track_e.append(e)
            self.track_n.append(n)
            if rec.get("mark"):
                self.marks.append((rec["mark"], e, n))
