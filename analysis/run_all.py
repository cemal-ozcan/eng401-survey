"""Runs all required tests (handout section 9) and exports GIS layers.

Example (sample data from the simulator):
  .venv/bin/python analysis/run_all.py --survey data/sample/mock_survey.csv \
      --telemetry data/sample/mock_telemetry.csv \
      --static data/sample/mock_static.csv --reference data/sample/mock_static_reference.csv \
      --route data/sample/mock_planned_route.csv --truth data/sample/mock_truth.csv \
      --out data/processed/analysis_sample

Field data: replace with the SURVEY_NNN.CSV files, the receiver log, the
reference-point file and the planned route from the Geomatics team.
Outputs: <out>/report.md, results.json, figures/*.png, gis/*.geojson
"""

import argparse
from pathlib import Path

from surveypc.analysis.report import Inputs, run


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--survey", type=Path, nargs="+", required=True, help="SD card CSV file(s)")
    ap.add_argument("--period-ms", type=float, default=1000.0, help="requested sampling period")
    ap.add_argument("--telemetry", type=Path, help="PC receiver CSV (latency, telemetry completeness)")
    ap.add_argument("--static", type=Path, nargs="*", default=[], help="CSV of a stationary measurement")
    ap.add_argument("--reference", type=Path, help="reference point for --static (id,lat,lon or id,easting,northing)")
    ap.add_argument("--marks-reference", type=Path, help="reference points in button-mark order")
    ap.add_argument("--mark-window", type=float, default=5.0, help="seconds around a mark to average")
    ap.add_argument("--route", type=Path, help="planned route: CSV (lat,lon) or GeoJSON LineString")
    ap.add_argument("--truth", type=Path, help="simulation ground truth (seq,lat,lon)")
    ap.add_argument("--out", type=Path, default=Path("data/processed/analysis"))
    a = ap.parse_args()
    run(Inputs(a.survey, a.period_ms, a.out, a.telemetry, a.static, a.reference, a.marks_reference,
               a.mark_window, a.route, a.truth))
    print(f"report: {a.out / 'report.md'}")


if __name__ == "__main__":
    main()
