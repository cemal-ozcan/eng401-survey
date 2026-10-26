"""Generate UTM reference vectors with pyproj (the independent implementation).

Output: shared/golden/utm_vectors.csv, consumed by the firmware native test
(test/test_utm) to verify our Krüger-series implementation.

Usage: .venv/bin/python tools/gen_utm_vectors.py
"""

import csv
from pathlib import Path

from pyproj import Transformer

# (lat, lon, expected_zone, forced) — forced=1 means "convert into this zone"
# even though it is not the standard zone for the point.
POINTS = [
    (39.92077, 32.85411, 36, 0),    # Ankara
    (41.00824, 28.97836, 35, 0),    # Istanbul
    (38.42373, 27.14283, 35, 0),    # Izmir
    (39.90000, 33.00000, 36, 0),    # on the central meridian of zone 36
    (39.90000, 35.99999, 36, 0),    # zone 36 east edge
    (39.90000, 36.00000, 37, 0),    # zone 37 west edge
    (39.90000, 30.00001, 36, 0),    # zone 36 west edge
    (0.0, 0.0, 31, 0),              # equator / Greenwich
    (0.0, 3.0, 31, 0),              # equator on central meridian
    (-33.86880, 151.20930, 56, 0),  # Sydney (southern hemisphere)
    (-0.00001, 33.0, 36, 0),        # just south of the equator
    (-79.9, -179.9, 1, 0),          # southern limit, zone 1
    (83.9, 179.9, 60, 0),           # northern limit, zone 60
    (60.0, 5.0, 32, 0),             # Norway exception 32V
    (78.2, 15.6, 33, 0),            # Svalbard 33X
    (78.2, 25.0, 35, 0),            # Svalbard 35X
    (39.90000, 36.50000, 36, 1),    # forced: 3.5 deg east of zone 36 CM
    (39.90000, 29.50000, 36, 1),    # forced: 3.5 deg west of zone 36 CM
]

OUT = Path(__file__).resolve().parents[1] / "shared" / "golden" / "utm_vectors.csv"


def main() -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["lat", "lon", "zone", "hemi", "forced", "easting", "northing"])
        for lat, lon, zone, forced in POINTS:
            hemi = "N" if lat >= 0 else "S"
            epsg = (32600 if hemi == "N" else 32700) + zone
            tr = Transformer.from_crs("EPSG:4326", f"EPSG:{epsg}", always_xy=True)
            e, n = tr.transform(lon, lat)
            w.writerow([f"{lat:.8f}", f"{lon:.8f}", zone, hemi, forced, f"{e:.6f}", f"{n:.6f}"])
    print(f"wrote {len(POINTS)} vectors to {OUT}")


if __name__ == "__main__":
    main()
