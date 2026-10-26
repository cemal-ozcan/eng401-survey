import math
from datetime import datetime, timezone

import pytest

from surveypc import nmea

RMC = "$GNRMC,101542.00,A,3955.24620,N,03251.24660,E,1.944,87.20,021126,,,A*4B"
GGA = "$GNGGA,101542.00,3955.24620,N,03251.24660,E,1,09,0.92,938.4,M,36.2,M,,*49"
VTG = "$GNVTG,87.20,T,,M,1.944,N,3.600,K,A*13"
RMC_SOUTH = "$GPRMC,235959.999,A,3352.1280,S,15112.5580,E,0.0,0.0,311299,,*15"


def test_checksum():
    assert nmea.checksum_ok(GGA)
    assert not nmea.checksum_ok(GGA.replace("938.4", "938.5"))
    assert not nmea.checksum_ok("$GPGGA,1,2")


def test_coordinates():
    assert nmea.parse_coordinate("3955.24620", "N") == pytest.approx(39.92077, abs=1e-9)
    assert nmea.parse_coordinate("15112.5580", "W") == pytest.approx(-151.2093, abs=1e-9)
    assert math.isnan(nmea.parse_coordinate("3975.0", "N"))
    assert math.isnan(nmea.parse_coordinate("", "N"))


def test_full_fix_sequence():
    p = nmea.Parser()
    assert [p.process(s) for s in (RMC, GGA, VTG)] == ["RMC", "GGA", "VTG"]
    d = p.data
    assert d.pos_valid and d.fix_quality == 1 and d.satellites == 9
    assert d.lat == pytest.approx(39.92077) and d.lon == pytest.approx(32.85411)
    assert d.hdop == pytest.approx(0.92) and d.alt == pytest.approx(938.4)
    assert d.speed_kmh == pytest.approx(3.6) and d.course_deg == pytest.approx(87.2)
    assert d.utc == datetime(2026, 11, 2, 10, 15, 42, tzinfo=timezone.utc)


def test_southern_and_fractional_time():
    p = nmea.Parser()
    p.process(RMC_SOUTH)
    assert p.data.lat == pytest.approx(-33.8688)
    assert p.data.time_of_day_ms == 86399999


def test_golden_stream_same_as_firmware(golden):
    # Same expectations as firmware test_nmea_parser::test_byte_stream_and_checksum_errors
    p = nmea.Parser()
    kinds = [p.process(line) for line in (golden / "nmea_sample.nmea").read_text().splitlines()]
    assert p.sentences == 6 and p.checksum_errors == 1
    assert kinds.count("GGA") == 2 and kinds.count("RMC") == 2
    assert not p.data.pos_valid
