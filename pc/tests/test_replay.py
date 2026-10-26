import pytest

from surveypc import protocol as p
from surveypc.replay import convert, detect, nmea_to_records, serial_to_records, utm_zone, write_survey
from surveypc.serial_log import split_prefix


def test_split_prefix():
    assert split_prefix("1793613600123\t$GNGGA,x") == (1793613600123, "$GNGGA,x")
    assert split_prefix("$GNGGA,x") == (None, "$GNGGA,x")
    assert split_prefix("12\tshort") == (None, "12\tshort")


def test_utm_zone_rules():
    assert utm_zone(39.9, 32.8) == 36
    assert utm_zone(60.0, 5.0) == 32
    assert utm_zone(78.0, 15.0) == 33
    assert utm_zone(0.0, 180.0) == 60


def test_nmea_golden_to_records(golden):
    lines = (golden / "nmea_sample.nmea").read_text().splitlines()
    recs, info = nmea_to_records([f"1793613600000\t{line}" for line in lines])  # with logger prefixes
    assert info == {"sentences": 6, "checksum_errors": 1}
    assert len(recs) == 2  # two valid GGA (third has a bad checksum)
    fix, nofix = recs
    assert fix["flags"] & p.FLAG_GNSS_FIX and fix["utm_zone"] == 36
    assert fix["easting"] == pytest.approx(487532.490, abs=1e-3)
    assert fix["utc"] == "2026-11-02T10:15:42.000Z" and fix["t_us"] == 0
    assert nofix["lat"] is None and nofix["sats"] == 3 and nofix["t_us"] == 1_000_000
    # round trip through the ICD codec
    for r in recs:
        assert p.parse_csv_line(p.encode_csv_line(r))["seq"] == r["seq"]


def test_serial_log_extraction(golden):
    csv_lines = (golden / "record_example.csv").read_text().splitlines()
    log = ["1793613600000\t# ENG401 survey firmware v0.2.0 (REAL sensors)",
           "1793613600001\t# INIT MPU6050 OK", "1793613600002\t" + csv_lines[0],
           "1793613601000\t" + csv_lines[1], "1793613601500\tets Jul 29 2019 12:21:46 garbage,line",
           "1793613602000\t" + csv_lines[2][:40]]  # cut-off line (cable pulled)
    recs, info = serial_to_records(log)
    assert [r["seq"] for r in recs] == [42]
    assert info["skipped_lines"] == 2
    assert detect(log) == "serial"


def test_convert_file_end_to_end(tmp_path, golden):
    src = tmp_path / "raw.txt"
    src.write_text((golden / "nmea_sample.nmea").read_text())
    recs, kind, _ = convert(src)
    assert kind == "nmea"
    out = tmp_path / "survey.csv"
    write_survey(recs, out)
    assert out.read_text().splitlines()[0] == p.CSV_HEADER
