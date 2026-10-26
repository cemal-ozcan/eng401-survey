"""The mock dataset produced by the firmware pipeline simulation must decode cleanly.

Regenerate: cd firmware && SIM_OUT=$PWD/../data/sample ../.venv/bin/pio test -e native -f test_pipeline_sim
"""

from pathlib import Path

from surveypc import protocol as p
from surveypc.utm_check import compare_records, read_survey_csv

SAMPLE = Path(__file__).resolve().parents[2] / "data" / "sample"


def test_mock_survey_decodes_and_is_complete():
    lines = (SAMPLE / "mock_survey.csv").read_text().splitlines()
    assert lines[0] == p.CSV_HEADER
    rows = [p.parse_csv_line(line) for line in lines[1:]]
    assert [r["seq"] for r in rows] == list(range(len(rows)))
    assert all(r["flags"] & p.FLAG_MOCK for r in rows)
    fix = sum(1 for r in rows if r["flags"] & p.FLAG_GNSS_FIX)
    assert fix == 554  # 600 s minus cold start (10 s) and 3 outages (36 s)


def test_mock_survey_utm_agrees_with_pyproj():
    res = compare_records(read_survey_csv(SAMPLE / "mock_survey.csv"))
    assert res.n == 554
    # CSV rounding only (lat/lon 8 decimals + E/N 3 decimals), see analysis/verify_utm.py
    assert res.max_mm < 1.5
    assert res.mean_mm < 0.8
