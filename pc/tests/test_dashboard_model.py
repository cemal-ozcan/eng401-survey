import math

from surveypc import dashboard_model as m
from surveypc import protocol as p


def golden_rows(golden):
    lines = (golden / "record_example.csv").read_text().splitlines()[1:]
    return [p.parse_csv_line(line) for line in lines]


def test_gnss_fields_with_fix(golden):
    full, _ = golden_rows(golden)
    f = m.gnss_fields(full)
    assert f["fix"] == m.Field("GPS fix", m.OK)
    assert f["sats"].level == m.OK
    assert f["hdop"] == m.Field("0.92", m.OK)
    assert f["utm"].text == "36N  E 487532.49  N 4418973.71"
    assert f["utc"].level == m.OK


def test_gnss_fields_without_fix(golden):
    _, degraded = golden_rows(golden)
    f = m.gnss_fields(degraded)
    assert f["fix"].level == m.BAD
    assert f["position"].text == "—"
    assert f["sats"].level == m.BAD  # 3 satellites
    assert f["hdop"].level == m.BAD  # 25.5
    assert f["utc"] == m.Field("not synced", m.WARN)


def test_sensor_and_flag_fields(golden):
    _, degraded = golden_rows(golden)
    s = m.sensor_fields(degraded)
    assert s["temp"] == m.Field("—", m.BAD)  # BME280 failed
    assert s["dist"] == m.Field("no target", m.WARN)
    assert s["accel"].level == m.OK
    flags = dict(m.flag_fields(degraded))
    assert flags["IMU"].level == m.OK and flags["BME280"].level == m.BAD


def test_hdop_levels():
    assert [m.hdop_level(x) for x in (None, 1.0, 3.0, 9.0)] == [m.NONE, m.OK, m.WARN, m.BAD]


def test_connection_and_completeness():
    assert m.connection_field({"seconds_since_rx": None}).level == m.NONE
    assert m.connection_field({"seconds_since_rx": 0.4, "device": ("10.0.0.5", 5006)}).level == m.OK
    assert m.connection_field({"seconds_since_rx": 9.0, "device": ("10.0.0.5", 5006)}).level == m.BAD
    snap = {"received": 97, "lost": 3, "completeness_pct": 97.0}
    assert m.completeness_field(snap).level == m.WARN


def test_model_track_marks_and_series(golden):
    full, degraded = golden_rows(golden)
    model = m.DashboardModel(history=10)
    model.add(full, latency_ms=25.0)
    marked = dict(full, seq=43, t_us=full["t_us"] + 1_000_000, easting=full["easting"] + 3.0, mark=1)
    model.add(marked)
    model.add(dict(degraded, t_us=full["t_us"] + 2_000_000))
    assert model.zone == "36N"
    assert len(model.track_e) == 2  # no-fix row is not part of the track
    assert model.track_e[1] - model.track_e[0] == 3.0
    assert model.marks == [(1, model.track_e[1], model.track_n[1])]
    assert list(model.t) == [0.0, 1.0, 2.0]
    assert math.isnan(model.series["temp"][2]) and math.isnan(model.series["latency"][1])
    assert model.series["latency"][0] == 25.0


def test_model_history_is_bounded(golden):
    full, _ = golden_rows(golden)
    model = m.DashboardModel(history=5)
    for i in range(20):
        model.add(dict(full, seq=i, t_us=full["t_us"] + i * 1_000_000))
    assert len(model.t) == 5 and len(model.series["az"]) == 5


def test_placeholder_hdop_is_not_plotted(golden):
    full, _ = golden_rows(golden)
    model = m.DashboardModel()
    model.add(dict(full, hdop=99.99))
    assert math.isnan(model.series["hdop"][0])
