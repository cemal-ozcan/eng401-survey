"""GIS products for QGIS (requirement R9, deliverable D4).

GeoJSON files are WGS84 lon/lat (RFC 7946) and open directly in QGIS:
  trajectory.geojson    LineString parts of the walked route (split where the fix was lost)
  points.geojson        every fix as a point with all attributes (for thematic maps)
  observations.geojson  button-marked observation points
  planned_route.geojson planned route (if given)
  points_utm.csv        the same points with UTM E/N, for "Add Delimited Text Layer" with EPSG:326zz/327zz
"""

from __future__ import annotations

import csv
import json
from pathlib import Path

from ..protocol import COLUMNS
from .data import Survey

POINT_ATTRS = ["seq", "utc", "fix", "sats", "hdop", "alt", "spd", "cog", "utm_zone", "utm_hemi", "easting",
               "northing", "temp", "pres", "hum", "dist", "az", "mark", "session"]


def _feature(geom: dict, props: dict) -> dict:
    return {"type": "Feature", "geometry": geom, "properties": props}


def _lonlat(r: dict) -> list[float]:
    return [round(r["lon"], 8), round(r["lat"], 8)]


def trajectory_parts(survey: Survey, max_gap_s: float = 5.0) -> list[list[dict]]:
    """Consecutive fixes; a new part starts after a fix gap > max_gap_s or a reboot."""
    parts: list[list[dict]] = []
    prev = None
    for r in survey.fixes():
        new = prev is None or r["session"] != prev["session"] or (r["t_us"] - prev["t_us"]) / 1e6 > max_gap_s
        if new:
            parts.append([])
        parts[-1].append(r)
        prev = r
    return [p for p in parts if len(p) >= 2]


def export(survey: Survey, out_dir: Path, planned_latlon: list[tuple[float, float]] | None = None,
           name: str = "survey") -> dict[str, Path]:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    zone = survey.zone()
    files: dict[str, Path] = {}

    def write(fname: str, features: list[dict], extra: dict | None = None) -> None:
        fc = {"type": "FeatureCollection", "name": f"{name}_{fname.split('.')[0]}", "features": features}
        if extra:
            fc.update(extra)
        path = out / fname
        path.write_text(json.dumps(fc, ensure_ascii=False, separators=(",", ":")))
        files[fname] = path

    parts = trajectory_parts(survey)
    write("trajectory.geojson", [
        _feature({"type": "LineString", "coordinates": [_lonlat(r) for r in part]},
                 {"part": i + 1, "start_seq": part[0]["seq"], "end_seq": part[-1]["seq"],
                  "start_utc": part[0]["utc"], "end_utc": part[-1]["utc"], "n_points": len(part)})
        for i, part in enumerate(parts)])

    fixes = survey.fixes()
    write("points.geojson", [_feature({"type": "Point", "coordinates": _lonlat(r)},
                                      {k: r.get(k) for k in POINT_ATTRS}) for r in fixes])
    write("observations.geojson", [_feature({"type": "Point", "coordinates": _lonlat(r)},
                                            {"name": f"P{r['mark']}", **{k: r.get(k) for k in POINT_ATTRS}})
                                   for r in fixes if r.get("mark")])
    if planned_latlon:
        write("planned_route.geojson", [_feature(
            {"type": "LineString", "coordinates": [[round(lon, 8), round(lat, 8)] for lat, lon in planned_latlon]},
            {"name": "planned route"})])

    csv_path = out / "points_utm.csv"
    with csv_path.open("w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(COLUMNS + ["session"])
        for r in fixes:
            w.writerow(["" if r.get(c) is None else r[c] for c in COLUMNS + ["session"]])
    files["points_utm.csv"] = csv_path
    if zone:
        epsg = (32600 if zone[1] == "N" else 32700) + zone[0]
        (out / "README.txt").write_text(
            f"GeoJSON layers are WGS84 (EPSG:4326).\n"
            f"points_utm.csv: QGIS > Layer > Add Delimited Text Layer, X=easting, Y=northing, CRS EPSG:{epsg}.\n")
    return files
