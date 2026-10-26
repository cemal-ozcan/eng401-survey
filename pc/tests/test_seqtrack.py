from surveypc.seqtrack import SeqTracker


def feed(seqs):
    t = SeqTracker()
    results = [t.add(s) for s in seqs]
    return t, results


def test_continuous():
    t, _ = feed(range(10))
    assert (t.received, t.lost, t.completeness_pct) == (10, 0, 100.0)


def test_gap_counts_lost():
    t, r = feed([0, 1, 2, 5, 6])
    assert r[3] == "gap"
    assert (t.received, t.lost, t.expected) == (5, 2, 7)
    assert t.completeness_pct == 100 * 5 / 7


def test_late_packet_is_not_lost():
    t, r = feed([0, 1, 3, 2, 4])
    assert r[3] == "late"
    assert (t.received, t.lost, t.out_of_order) == (5, 0, 1)


def test_duplicate():
    t, r = feed([0, 1, 1, 2])
    assert r[2] == "dup"
    assert (t.received, t.duplicates) == (3, 1)


def test_reboot_restarts_counting():
    t, r = feed([500, 501, 502, 0, 1, 2])
    assert r[3] == "reboot"
    assert (t.reboots, t.received, t.lost) == (1, 6, 0)


def test_start_mid_stream():
    t, _ = feed([1000, 1001, 1003])
    assert (t.first_seq, t.lost) == (1000, 1)
