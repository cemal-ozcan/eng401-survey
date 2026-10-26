import pytest

from surveypc.clocksync import ClockSync, RunningStats, sync_sample

OFFSET = 1_000_000_000  # device clock - PC clock (µs)


def exchange(t1, up, down, proc=150):
    """Simulate a ping: up/down = one-way delays (µs)."""
    t2 = t1 + up + OFFSET
    t3 = t2 + proc
    t4 = t3 - OFFSET + down
    return t1, t2, t3, t4


def test_symmetric_delay_gives_exact_offset():
    s = sync_sample(*exchange(10_000, 3000, 3000))
    assert s.rtt_us == 6000
    assert s.offset_us == OFFSET


def test_asymmetry_error_is_half_difference():
    s = sync_sample(*exchange(10_000, 1000, 9000))
    assert s.rtt_us == 10_000
    assert s.offset_us == OFFSET + (1000 - 9000) / 2  # within ±rtt/2


def test_best_sample_has_smallest_rtt():
    cs = ClockSync()
    for i, (up, down) in enumerate([(9000, 2000), (800, 900), (5000, 5000)]):
        cs.add(*exchange(i * 1_000_000, up, down))
    assert cs.best().rtt_us == 1700
    assert cs.offset_us == pytest.approx(OFFSET - 50)
    assert cs.uncertainty_us == 850


def test_latency_of_data_packet():
    cs = ClockSync()
    cs.add(*exchange(0, 500, 500))
    sample_time_device = 5_000_000 + OFFSET  # sampled at PC time 5.000 s
    assert cs.latency_us(sample_time_device, 5_030_000) == pytest.approx(30_000)


def test_unsynced_and_reset():
    cs = ClockSync()
    assert cs.latency_us(1, 2) is None
    cs.add(*exchange(0, 500, 500))
    cs.reset()
    assert not cs.synced


def test_negative_rtt_is_ignored():
    cs = ClockSync()
    cs.add(0, 10, 20, -100)
    assert not cs.synced


def test_running_stats():
    rs = RunningStats()
    for x in [10, 20, 30, 40]:
        rs.add(x)
    assert (rs.min, rs.max, rs.mean) == (10, 40, 25)
    assert rs.std == pytest.approx(12.909944, rel=1e-6)
