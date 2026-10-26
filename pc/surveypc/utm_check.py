"""Independent verification of device UTM coordinates with pyproj (handout section 9)."""

from __future__ import annotations

import csv
import math
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Iterable

from pyproj import Transformer

from .protocol import Record, parse_csv_line


@lru_cache(maxsize=None)
def _transformer(zone: int, hemi: str) -> Transformer:
    epsg = (32600 if hemi == "N" else 32700) + zone
    return Transformer.from_crs("EPSG:4326", f"EPSG:{epsg}", always_xy=True)


def reference_utm(lat: float, lon: float, zone: int, hemi: str) -> tuple[float, float]:
    """pyproj (PROJ) easting/northing of a WGS84 point in the given zone."""
    return _transformer(zone, hemi).transform(lon, lat)


@dataclass
class UtmComparison:
    n: int
    mean_mm: float
    rmse_mm: float
    max_mm: float
    max_row_seq: int | None
    # per-row (seq, lat, lon, dE_mm, dN_mm, d_mm)
    rows: list[tuple[int, float, float, float, float, float]]


def compare_records(records: Iterable[Record]) -> UtmComparison:
    """Compare device easting/northing with pyproj for every record that has both."""
    rows = []
    for r in records:
        if None in (r.get("lat"), r.get("lon"), r.get("easting"), r.get("northing"), r.get("utm_zone")):
            continue
        e_ref, n_ref = reference_utm(r["lat"], r["lon"], r["utm_zone"], r["utm_hemi"])
        de = (r["easting"] - e_ref) * 1000.0
        dn = (r["northing"] - n_ref) * 1000.0
        rows.append((r["seq"], r["lat"], r["lon"], de, dn, math.hypot(de, dn)))
    if not rows:
        return UtmComparison(0, math.nan, math.nan, math.nan, None, [])
    d = [row[5] for row in rows]
    worst = max(rows, key=lambda row: row[5])
    return UtmComparison(
        n=len(rows),
        mean_mm=sum(d) / len(d),
        rmse_mm=math.sqrt(sum(x * x for x in d) / len(d)),
        max_mm=worst[5],
        max_row_seq=worst[0],
        rows=rows,
    )


def read_survey_csv(path: Path) -> list[Record]:
    """Read an SD-card survey CSV (header + data rows)."""
    with Path(path).open() as f:
        lines = f.read().splitlines()
    return [parse_csv_line(line) for line in lines[1:] if line.strip()]


def write_comparison_csv(result: UtmComparison, path: Path) -> None:
    with Path(path).open("w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["seq", "lat", "lon", "dE_mm", "dN_mm", "d_mm"])
        for seq, lat, lon, de, dn, dd in result.rows:
            w.writerow([seq, f"{lat:.8f}", f"{lon:.8f}", f"{de:.3f}", f"{dn:.3f}", f"{dd:.3f}"])
