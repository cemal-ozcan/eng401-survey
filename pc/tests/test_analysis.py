"""Analysis functions on synthetic data with known answers, plus the sample run."""

import json
import math
from pathlib import Path

import numpy as np
import pytest

from surveypc import protocol as p
from surveypc.analysis import gis, tests
from surveypc.analysis.data import Survey, latlon_to_utm, load_points
from surveypc.analysis.report import Inputs, run
from surveypc.analysis.stats import describe, error_stats

SAMPLE = Path(__file__).resolve().parents[2] / "data" / "sample"
ALL_OK = p.FLAG_GNSS_FIX | p.FLAG_IMU_OK | p.FLAG_ENV_OK | p.FLAG_DIST_OK
E0, N0 = 481200.0, 4415500.0


def row(seq, t_s, e=E0, n=N0, flags=ALL_OK, mark=0, session=0, **kw):
    lat, lon = 39.89, 32.78
    r = {c: None for c in p.COLUMNS}
    r.update(seq=seq, t_us=int(t_s * 1e6), fix=1 if flags & p.FLAG_GNSS_FIX else 0, sats=9, hdop=1.0,
             lat=lat, lon=lon, utm_zone=36, utm_hemi="N", easting=e, northing=n, temp=20.0, pres=900.0,
             hum=40.0, dist=1500, ax=0.0, ay=0.0, az=9.8, gx=0.0, gy=0.0, gz=0.0, flags=flags, missed=0,
             mark=mark, session=session)
    r.update(kw)
    return r


def test_error_stats_known_values():
    s = error_stats([3, 0, -3, 0], [4, 0, -4, 0])  # d = 5, 0, 5, 0
    assert s["mean_error_m"] == 2.5
    assert s["rmse_m"] == pytest.approx(math.sqrt(12.5))
    assert s["max_error_m"] == 5
    assert s["bias_e_m"] == 0 and s["bias_n_m"] == 0
    assert s["std_m"] == pytest.approx(np.std([5, 0, 5, 0], ddof=1))


def test_describe_ignores_nan():
    d = describe([1.0, np.nan, 3.0])
    assert (d["n"], d["mean"], d["range"]) == (2, 2.0, 2.0)
    assert describe([])["n"] == 0


def test_static_accuracy():
    rows = [row(i, i, e=E0 + (1 if i % 2 else -1), n=N0 + 2) for i in range(10)]
    g = tests.gnss_static(Survey(rows), E0, N0)
    assert g["errors"]["bias_n_m"] == pytest.approx(2.0)
    assert g["errors"]["mean_error_m"] == pytest.approx(math.sqrt(5))


def test_marks_accuracy_averages_window():
    rows = [row(i, i, e=E0 + 10 + (0.5 if i % 2 else -0.5), n=N0) for i in range(20)]
    rows[10]["mark"] = 1
    refs = [{"id": "A", "easting": E0 + 10, "northing": N0 + 1}, {"id": "B", "easting": 0, "northing": 0}]
    g = tests.gnss_marks(Survey(rows), refs, window_s=2.0)
    a, b = g["points"]
    assert a["status"] == "ok" and a["n_fixes"] == 5
    assert a["dn_m"] == pytest.approx(-1.0) and abs(a["de_m"]) < 0.11
    assert b["status"] == "no mark recorded"


def test_sampling_missed_and_sessions():
    t = [0, 1, 2, 4, 5]  # one tick missing between 2 and 4
    rows = [row(i, ti) for i, ti in enumerate(t)] + [row(0, 0.0, session=1), row(1, 1.0, session=1)]
    sp = tests.sampling_performance(Survey(rows), 1000)
    assert sp["missed_samples"] == 1
    assert sp["actual_ms"]["n"] == 5  # intervals within sessions only
    assert sp["actual_ms"]["max"] == 2000


def test_completeness_levels():
    rows = [row(0, 0), row(1, 1, flags=ALL_OK & ~p.FLAG_GNSS_FIX), row(3, 3, flags=ALL_OK & ~p.FLAG_ENV_OK)]
    c = tests.completeness(Survey(rows), 1000)
    assert c["expected"] == 4
    assert c["levels"]["records_present"]["pct"] == 75.0
    assert c["levels"]["gnss_fix"]["count"] == 2
    assert c["levels"]["fully_valid"]["count"] == 1


def test_route_cross_track():
    planned = [{"easting": E0, "northing": N0}, {"easting": E0 + 100, "northing": N0}]
    rows = [row(0, 0, e=E0 + 50, n=N0 + 3), row(1, 1, e=E0 + 120, n=N0), row(2, 2, e=E0 + 10, n=N0 - 1)]
    rc = tests.route_comparison(Survey(rows), planned)
    assert list(rc["_series"]["d"]) == pytest.approx([3.0, 20.0, 1.0])  # beyond the end -> distance to endpoint
    assert rc["planned_length_m"] == 100.0
    assert rc["within_m"]["2"] == pytest.approx(100 / 3)


def test_load_points_csv_and_geojson(tmp_path):
    (tmp_path / "p.csv").write_text("id,lat,lon\nA,39.92077,32.85411\n")
    pts = load_points(tmp_path / "p.csv", 36, "N")
    assert pts[0]["easting"] == pytest.approx(487532.490, abs=1e-3)
    gj = {"type": "Feature", "geometry": {"type": "LineString", "coordinates": [[32.85411, 39.92077], [32.86, 39.93]]}}
    (tmp_path / "r.geojson").write_text(json.dumps(gj))
    assert len(load_points(tmp_path / "r.geojson", 36, "N")) == 2


def test_gis_export_structure(tmp_path):
    e, n = latlon_to_utm([39.89, 39.8901], [32.78, 32.7801], 36, "N")
    rows = [row(i, i, lat=39.89 + i * 1e-5, lon=32.78 + i * 1e-5) for i in range(5)]
    rows += [row(5, 20, lat=39.9, lon=32.79, mark=1), row(6, 21, lat=39.9001, lon=32.7901)]  # after a 15 s gap
    files = gis.export(Survey(rows), tmp_path)
    traj = json.loads(files["trajectory.geojson"].read_text())
    assert len(traj["features"]) == 2  # split at the gap
    lon, lat = traj["features"][0]["geometry"]["coordinates"][0]
    assert (lon, lat) == (32.78, 39.89)  # RFC 7946 order: lon, lat
    obs = json.loads(files["observations.geojson"].read_text())
    assert obs["features"][0]["properties"]["name"] == "P1"
    pts = json.loads(files["points.geojson"].read_text())
    assert len(pts["features"]) == 7 and "temp" in pts["features"][0]["properties"]
    assert "EPSG:32636" in (tmp_path / "README.txt").read_text()


def test_full_report_on_sample(tmp_path):
    res = run(Inputs(survey=[SAMPLE / "mock_survey.csv"], out=tmp_path, telemetry=SAMPLE / "mock_telemetry.csv",
                     static=[SAMPLE / "mock_static.csv"], reference=SAMPLE / "mock_static_reference.csv",
                     route=SAMPLE / "mock_planned_route.csv", truth=SAMPLE / "mock_truth.csv"))
    static, truth = res["gnss_accuracy"]
    # Mock GNSS: 1.5 m white noise per axis + bounded drifting bias (<= 2 m)
    assert 1.5 < static["errors"]["rmse_m"] < 3.5
    assert 1.5 < truth["errors"]["rmse_m"] < 3.5
    assert res["utm_verification"]["max_mm"] < 1.5
    assert res["sensor_repeatability"]["dist"]["std"] == pytest.approx(4.0, abs=0.6)  # mock ToF noise 4 mm
    assert res["completeness"]["levels"]["gnss_fix"]["count"] == 554
    assert res["route"]["within_m"]["5"] > 95.0
    for name in ("report.md", "results.json", "figures/track_vs_route.png", "gis/trajectory.geojson"):
        assert (tmp_path / name).exists()
    json.loads((tmp_path / "results.json").read_text())
