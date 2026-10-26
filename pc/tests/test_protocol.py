"""The PC codec must read and reproduce the firmware's golden files byte for byte."""

import json

import pytest

from surveypc import protocol as p


def _csv_rows(golden):
    lines = (golden / "record_example.csv").read_text().splitlines()
    return lines[0], lines[1:]


def _json_rows(golden):
    return (golden / "record_example.jsonl").read_text().splitlines()


def test_header_matches_firmware(golden):
    header, _ = _csv_rows(golden)
    assert header == p.CSV_HEADER


def test_full_row_values(golden):
    _, rows = _csv_rows(golden)
    r = p.parse_csv_line(rows[0])
    assert r["seq"] == 42 and r["fix"] == 1 and r["sats"] == 9
    assert r["utc"] == "2026-11-02T10:15:42.000Z"
    assert r["lat"] == pytest.approx(39.92077)
    assert (r["utm_zone"], r["utm_hemi"]) == (36, "N")
    assert r["easting"] == pytest.approx(487532.490)
    assert r["flags"] & p.FLAG_GNSS_FIX


def test_degraded_row_uses_none(golden):
    _, rows = _csv_rows(golden)
    r = p.parse_csv_line(rows[1])
    assert r["utc"] is None and r["lat"] is None and r["utm_zone"] is None and r["temp"] is None
    assert r["dist"] == 0 and r["missed"] == 2 and r["mark"] == 1
    assert not r["flags"] & p.FLAG_GNSS_FIX


@pytest.mark.parametrize("row", [0, 1])
def test_csv_and_json_round_trip_byte_exact(golden, row):
    _, csv_rows = _csv_rows(golden)
    json_rows = _json_rows(golden)
    rec = p.parse_csv_line(csv_rows[row])
    assert p.encode_csv_line(rec) == csv_rows[row]
    assert p.encode_data_json(rec) == json_rows[row]
    decoded = p.decode_packet(json_rows[row].encode())
    assert {k: decoded[k] for k in p.COLUMNS} == rec


def test_decode_rejects_bad_packets():
    with pytest.raises(p.ProtocolError):
        p.decode_packet(b"not json")
    with pytest.raises(p.ProtocolError):
        p.decode_packet(json.dumps({"v": 2, "type": "data"}))
    with pytest.raises(p.ProtocolError):
        p.decode_packet(json.dumps({"v": 1, "type": "data", "seq": 1}))


def test_heartbeat_passes_through():
    hb = p.decode_packet(b'{"v":1,"type":"hb","t_us":1,"uptime_s":0}')
    assert hb["type"] == "hb"


def test_wrong_column_count():
    with pytest.raises(p.ProtocolError):
        p.parse_csv_line("1,2,3")
