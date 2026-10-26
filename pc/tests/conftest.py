from pathlib import Path

import pytest

GOLDEN = Path(__file__).resolve().parents[2] / "shared" / "golden"


@pytest.fixture
def golden() -> Path:
    return GOLDEN
