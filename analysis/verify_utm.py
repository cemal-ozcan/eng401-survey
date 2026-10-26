"""Required test: compare device UTM output with an independent implementation (pyproj).

Usage:
    .venv/bin/python analysis/verify_utm.py data/raw/SURVEY_001.CSV [--out data/processed/utm_check.csv]

Note: the device prints easting/northing with 3 decimals, so differences up to
~0.7 mm are pure rounding; the algorithm itself agrees to < 0.001 mm
(firmware native test test_utm).
"""

import argparse
import sys
from pathlib import Path

from surveypc.utm_check import compare_records, read_survey_csv, write_comparison_csv


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("survey_csv", type=Path)
    ap.add_argument("--out", type=Path, help="write per-row differences to this CSV")
    ap.add_argument("--max-mm", type=float, default=1.0, help="pass threshold (default 1 mm)")
    args = ap.parse_args()

    res = compare_records(read_survey_csv(args.survey_csv))
    if res.n == 0:
        print("no rows with both geographic and UTM coordinates")
        return 2
    print(f"UTM verification vs pyproj ({args.survey_csv.name})")
    print(f"  points compared : {res.n}")
    print(f"  mean difference : {res.mean_mm:.3f} mm")
    print(f"  RMSE            : {res.rmse_mm:.3f} mm")
    print(f"  max difference  : {res.max_mm:.3f} mm (seq {res.max_row_seq})")
    ok = res.max_mm <= args.max_mm
    print(f"  result          : {'PASS' if ok else 'FAIL'} (threshold {args.max_mm} mm)")
    if args.out:
        write_comparison_csv(res, args.out)
        print(f"  details         : {args.out}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
