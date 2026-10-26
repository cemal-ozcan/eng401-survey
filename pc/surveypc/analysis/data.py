"""Loading survey / telemetry CSV files and coordinate helpers."""

from __future__ import annotations

import csv
import json
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

import numpy as np
from pyproj import Transformer

from ..protocol import FLAG_GNSS_FIX, parse_csv_line


@dataclass
class Survey:
    """Records of one or more SD files, split into sessions at device reboots."""

    rows: list[dict]

    @classmethod
    def load(cls, *paths: Path) -> "Survey":
        rows: list[dict] = []
        session = -1
        for path in paths:
            last_seq = None
            for line in Path(path).read_text().splitlines()[1:]:
                if not line.strip() or line.startswith("#"):
                    continue
                r = parse_csv_line(line)
                if last_seq is None or r["seq"] < last_seq:  # new file or reboot
                    session += 1
                r["session"] = session
                last_seq = r["seq"]
                rows.append(r)
        return cls(_dedupe(rows))

    def fixes(self) -> list[dict]:
        return [r for r in self.rows if (r["flags"] or 0) & FLAG_GNSS_FIX and r["easting"] is not None]

    def sessions(self) -> list[list[dict]]:
        out: dict[int, list[dict]] = {}
        for r in self.rows:
            out.setdefault(r["session"], []).append(r)
        return list(out.values())

    def zone(self) -> tuple[int, str] | None:
        f = self.fixes()
        return (f[0]["utm_zone"], f[0]["utm_hemi"]) if f else None


def _dedupe(rows: list[dict]) -> list[dict]:
    """Drop duplicate (session, seq) rows (possible after an SD write retry)."""
    seen, out = set(), []
    for r in rows:
        key = (r["session"], r["seq"])
        if key not in seen:
            seen.add(key)
            out.append(r)
    return out


def column(rows: list[dict], name: str) -> np.ndarray:
    """Column as float array, None -> NaN."""
    return np.array([np.nan if r.get(name) is None else float(r[name]) for r in rows], dtype=float)


def read_csv_dicts(path: Path) -> list[dict]:
    with Path(path).open() as f:
        return list(csv.DictReader(f))


@lru_cache(maxsize=None)
def _to_utm(zone: int, hemi: str) -> Transformer:
    epsg = (32600 if hemi == "N" else 32700) + zone
    return Transformer.from_crs("EPSG:4326", f"EPSG:{epsg}", always_xy=True)


def latlon_to_utm(lat, lon, zone: int, hemi: str):
    """pyproj (independent of the firmware) WGS84 -> UTM in a given zone."""
    lon_a = np.atleast_1d(np.asarray(lon, float))
    lat_a = np.atleast_1d(np.asarray(lat, float))
    if lon_a.size == 1:  # pyproj treats size-1 arrays as scalars (NumPy deprecation)
        e, n = _to_utm(zone, hemi).transform(float(lon_a[0]), float(lat_a[0]))
        return np.array([e]), np.array([n])
    e, n = _to_utm(zone, hemi).transform(lon_a, lat_a)
    return np.asarray(e), np.asarray(n)


def load_points(path: Path, zone: int, hemi: str) -> list[dict]:
    """Reference / route points from CSV with lat,lon or easting,northing columns
    (optional id), or a GeoJSON LineString / Point collection. Returns dicts with
    id, easting, northing in the given UTM zone."""
    path = Path(path)
    if path.suffix.lower() in (".geojson", ".json"):
        coords = _geojson_coords(json.loads(path.read_text()))
        ids = [str(i) for i in range(len(coords))]
        lat = [c[1] for c in coords]
        lon = [c[0] for c in coords]
    else:
        rows = read_csv_dicts(path)
        ids = [r.get("id") or r.get("vertex") or str(i) for i, r in enumerate(rows)]
        if rows and "easting" in rows[0]:
            return [{"id": i, "easting": float(r["easting"]), "northing": float(r["northing"])}
                    for i, r in zip(ids, rows)]
        lat = [float(r["lat"]) for r in rows]
        lon = [float(r["lon"]) for r in rows]
    e, n = latlon_to_utm(lat, lon, zone, hemi)
    return [{"id": i, "easting": float(a), "northing": float(b)} for i, a, b in zip(ids, np.atleast_1d(e), np.atleast_1d(n))]


def _geojson_coords(gj: dict) -> list[list[float]]:
    if gj.get("type") == "FeatureCollection":
        out = []
        for feat in gj["features"]:
            out += _geojson_coords(feat)
        return out
    if gj.get("type") == "Feature":
        return _geojson_coords(gj["geometry"])
    if gj["type"] == "Point":
        return [gj["coordinates"]]
    if gj["type"] == "LineString":
        return gj["coordinates"]
    if gj["type"] == "MultiLineString":
        return [c for line in gj["coordinates"] for c in line]
    raise ValueError(f"unsupported GeoJSON geometry {gj['type']}")
