import pyproj

import surveypc


def test_package_has_version():
    assert surveypc.__version__


def test_pyproj_reference_available():
    # pyproj is the independent UTM reference required by the handout (section 9).
    assert pyproj.CRS.from_epsg(32636).name == "WGS 84 / UTM zone 36N"
