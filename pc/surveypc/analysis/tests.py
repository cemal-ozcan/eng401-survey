"""The required tests of handout section 9 (UTM verification lives in utm_check)."""

from __future__ import annotations

import math

import numpy as np

from ..protocol import FLAG_DIST_OK, FLAG_ENV_OK, FLAG_GNSS_FIX, FLAG_IMU_OK
from .data import Survey, column, latlon_to_utm, read_csv_dicts
from .stats import describe, error_stats

# ---------------------------------------------------------------- GNSS accuracy


def gnss_quality(rows: list[dict]) -> dict:
    """Satellite count and HDOP over the fixes used."""
    return {"satellites": describe(column(rows, "sats")), "hdop": describe(column(rows, "hdop"))}


def gnss_static(survey: Survey, ref_e: float, ref_n: float) -> dict:
    """Device kept on one known point: every fix is compared with it."""
    fx = survey.fixes()
    de = column(fx, "easting") - ref_e
    dn = column(fx, "northing") - ref_n
    return {"mode": "static", "errors": error_stats(de, dn), "quality": gnss_quality(fx),
            "_series": {"seq": column(fx, "seq"), "de": de, "dn": dn}}


def gnss_marks(survey: Survey, references: list[dict], window_s: float = 5.0) -> dict:
    """Observation points: the k-th button mark (mark == k) belongs to references[k-1].
    The fixes within +-window_s of the mark are averaged and compared."""
    fx = survey.fixes()
    t = column(fx, "t_us") / 1e6
    points = []
    for k, ref in enumerate(references, start=1):
        mark_rows = [r for r in survey.rows if r.get("mark") == k]
        if not mark_rows:
            points.append({"id": ref["id"], "mark": k, "status": "no mark recorded"})
            continue
        tm = mark_rows[0]["t_us"] / 1e6
        sel = [r for r, ti in zip(fx, t) if abs(ti - tm) <= window_s and r["session"] == mark_rows[0]["session"]]
        if not sel:
            points.append({"id": ref["id"], "mark": k, "status": "no fix near mark"})
            continue
        e, n = column(sel, "easting").mean(), column(sel, "northing").mean()
        points.append({"id": ref["id"], "mark": k, "status": "ok", "n_fixes": len(sel),
                       "easting": float(e), "northing": float(n),
                       "de_m": float(e - ref["easting"]), "dn_m": float(n - ref["northing"]),
                       "d_m": float(math.hypot(e - ref["easting"], n - ref["northing"]))})
    ok = [p for p in points if p["status"] == "ok"]
    return {"mode": "marks", "window_s": window_s, "points": points,
            "errors": error_stats([p["de_m"] for p in ok], [p["dn_m"] for p in ok]),
            "quality": gnss_quality(fx)}


def gnss_truth(survey: Survey, truth_csv) -> dict:
    """Simulation only: per-record ground truth (seq, lat, lon)."""
    zone = survey.zone()
    truth = {int(r["seq"]): (float(r["lat"]), float(r["lon"])) for r in read_csv_dicts(truth_csv)}
    fx = [r for r in survey.fixes() if r["seq"] in truth]
    lat = [truth[r["seq"]][0] for r in fx]
    lon = [truth[r["seq"]][1] for r in fx]
    te, tn = latlon_to_utm(lat, lon, *zone)
    de = column(fx, "easting") - te
    dn = column(fx, "northing") - tn
    return {"mode": "truth", "errors": error_stats(de, dn), "quality": gnss_quality(fx),
            "_series": {"seq": column(fx, "seq"), "de": de, "dn": dn}}


# ---------------------------------------------------------------- sensors

REPEATABILITY_COLUMNS = {
    "temp": ("°C", FLAG_ENV_OK), "pres": ("hPa", FLAG_ENV_OK), "hum": ("%RH", FLAG_ENV_OK),
    "dist": ("mm", FLAG_DIST_OK),
    "ax": ("m/s²", FLAG_IMU_OK), "ay": ("m/s²", FLAG_IMU_OK), "az": ("m/s²", FLAG_IMU_OK),
    "gx": ("°/s", FLAG_IMU_OK), "gy": ("°/s", FLAG_IMU_OK), "gz": ("°/s", FLAG_IMU_OK),
}


def sensor_repeatability(survey: Survey, start_s: float | None = None, end_s: float | None = None) -> dict:
    """Stationary measurements: mean, std, min, max, range per sensor channel,
    using only rows whose sensor flag is OK, optionally within [start_s, end_s]
    seconds from the first record."""
    rows = survey.rows
    t0 = rows[0]["t_us"] / 1e6 if rows else 0.0
    sel = [r for r in rows if (start_s is None or r["t_us"] / 1e6 - t0 >= start_s)
           and (end_s is None or r["t_us"] / 1e6 - t0 <= end_s)]
    out = {}
    for name, (unit, flag) in REPEATABILITY_COLUMNS.items():
        valid = [r for r in sel if (r["flags"] or 0) & flag]
        out[name] = {"unit": unit, **describe(column(valid, name))}
    return out


# ---------------------------------------------------------------- timing


def sampling_performance(survey: Survey, period_ms: float) -> dict:
    """Requested vs actual sampling interval from consecutive t_us (device clock)."""
    intervals, missed = [], 0
    for rows in survey.sessions():
        t = column(rows, "t_us")
        dt = np.diff(t) / 1000.0
        intervals.append(dt)
        missed += int(np.sum(np.maximum(np.rint(dt / period_ms) - 1, 0)))
    dt = np.concatenate(intervals) if intervals else np.array([])
    d = describe(dt)
    jitter = np.abs(dt - period_ms)
    return {"requested_ms": period_ms, "actual_ms": d, "mean_abs_jitter_ms": float(jitter.mean()) if dt.size else None,
            "max_abs_jitter_ms": float(jitter.max()) if dt.size else None, "missed_samples": missed,
            "firmware_missed_counter": max((r["missed"] or 0) for r in survey.rows) if survey.rows else 0,
            "_series": {"dt_ms": dt}}


def latency(telemetry_csv) -> dict:
    """Sensor-to-PC latency from the receiver log (latency_ms column)."""
    rows = read_csv_dicts(telemetry_csv)
    lat = np.array([float(r["latency_ms"]) for r in rows if r.get("latency_ms")], float)
    d = describe(lat)
    if lat.size:
        d["p95"] = float(np.percentile(lat, 95))
    return {"latency_ms": d, "records": len(rows), "without_clock_sync": len(rows) - int(lat.size),
            "_series": {"latency_ms": lat}}


# ---------------------------------------------------------------- completeness


def completeness(survey: Survey, period_ms: float) -> dict:
    """valid observations / expected observations x 100.

    expected = (t_last - t_first) / period + 1 per session (time-based, also
    catches records that never got a sequence number). Several validity levels
    are reported because "valid" depends on what the observation is used for."""
    expected = 0
    for rows in survey.sessions():
        expected += int(round((rows[-1]["t_us"] - rows[0]["t_us"]) / 1000.0 / period_ms)) + 1
    rows = survey.rows

    def count(flag_mask: int) -> int:
        return sum(1 for r in rows if ((r["flags"] or 0) & flag_mask) == flag_mask)

    levels = {
        "records_present": len(rows),
        "gnss_fix": count(FLAG_GNSS_FIX),
        "imu_ok": count(FLAG_IMU_OK),
        "env_ok": count(FLAG_ENV_OK),
        "distance_ok": count(FLAG_DIST_OK),
        "fully_valid": count(FLAG_GNSS_FIX | FLAG_IMU_OK | FLAG_ENV_OK | FLAG_DIST_OK),
    }
    return {"expected": expected,
            "levels": {k: {"count": v, "pct": 100.0 * v / expected if expected else 0.0} for k, v in levels.items()}}


def telemetry_completeness(telemetry_csv) -> dict:
    """Received / expected packets from the sequence numbers in the receiver log."""
    seqs = sorted({int(r["seq"]) for r in read_csv_dicts(telemetry_csv)})
    if not seqs:
        return {"expected": 0}
    expected = seqs[-1] - seqs[0] + 1
    return {"expected": expected, "received": len(seqs), "lost": expected - len(seqs),
            "pct": 100.0 * len(seqs) / expected}


# ---------------------------------------------------------------- route


def _point_to_polyline(e: np.ndarray, n: np.ndarray, ve: np.ndarray, vn: np.ndarray) -> np.ndarray:
    """Shortest distance of each point to a polyline (vertices ve, vn), metres."""
    best = np.full(e.shape, np.inf)
    for i in range(len(ve) - 1):
        ax, ay, bx, by = ve[i], vn[i], ve[i + 1], vn[i + 1]
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy
        t = np.zeros_like(e) if L2 == 0 else np.clip(((e - ax) * dx + (n - ay) * dy) / L2, 0.0, 1.0)
        best = np.minimum(best, np.hypot(e - (ax + t * dx), n - (ay + t * dy)))
    return best


def route_comparison(survey: Survey, planned: list[dict], tolerances=(2.0, 5.0)) -> dict:
    """Cross-track distance of every fix from the planned route, and lengths."""
    fx = survey.fixes()
    e, n = column(fx, "easting"), column(fx, "northing")
    ve = np.array([p["easting"] for p in planned])
    vn = np.array([p["northing"] for p in planned])
    d = _point_to_polyline(e, n, ve, vn)
    res = {
        "cross_track_m": {**describe(d), "rmse": float(np.sqrt(np.mean(d ** 2))) if d.size else None,
                          "p95": float(np.percentile(d, 95)) if d.size else None},
        "within_m": {f"{tol:g}": float(100.0 * np.mean(d <= tol)) if d.size else None for tol in tolerances},
        "planned_length_m": float(np.sum(np.hypot(np.diff(ve), np.diff(vn)))),
        "measured_length_m": float(np.sum(np.hypot(np.diff(e), np.diff(n)))) if e.size else 0.0,
        "_series": {"e": e, "n": n, "d": d, "ve": ve, "vn": vn},
    }
    return res
