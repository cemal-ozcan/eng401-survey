import csv

import pytest

from surveypc import protocol as p
from surveypc.utm_check import compare_records, read_survey_csv, reference_utm


def test_reference_matches_golden_vectors(golden):
    with (golden / "utm_vectors.csv").open() as f:
        for row in csv.DictReader(f):
            e, n = reference_utm(float(row["lat"]), float(row["lon"]), int(row["zone"]), row["hemi"])
            assert e == pytest.approx(float(row["easting"]), abs=1e-6)
            assert n == pytest.approx(float(row["northing"]), abs=1e-6)


def test_golden_record_passes(golden):
    res = compare_records(read_survey_csv(golden / "record_example.csv"))
    assert res.n == 1  # the degraded row has no position and is skipped
    assert res.max_mm < 1.0  # 3-decimal rounding only


def test_detects_wrong_easting(golden):
    rows = read_survey_csv(golden / "record_example.csv")
    rows[0]["easting"] += 0.5
    res = compare_records(rows)
    assert res.max_mm == pytest.approx(500.0, abs=1.0)


def test_empty_input():
    rec = p.parse_csv_line((",".join([""] * len(p.FIELDS))))
    assert compare_records([rec]).n == 0
