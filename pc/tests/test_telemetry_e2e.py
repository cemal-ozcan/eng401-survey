"""Virtual ESP32 -> UDP (localhost) -> TelemetryReceiver, with injected loss and delay."""

import csv
import time

import pytest

from surveypc.receiver import LOG_COLUMNS, TelemetryReceiver
from surveypc.virtual_esp32 import VirtualEsp32


def test_loss_latency_and_csv(tmp_path):
    out = tmp_path / "telemetry.csv"
    with TelemetryReceiver(listen_port=0, out_csv=out, ping_interval_s=0.2, bind_host="127.0.0.1") as rx:
        dev = VirtualEsp32(pc_port=rx.listen_port, rate_hz=25, loss=0.1, delay_ms=40, jitter_ms=5,
                           seed=3, loop=True)
        with dev:
            time.sleep(3.0)
        time.sleep(0.2)  # let delayed packets arrive
        snap = rx.snapshot()

    assert snap["pongs"] >= 5
    # Receiver-side loss accounting must match what the device dropped (seq gaps);
    # packets dropped after the last received one are invisible, allow a margin of 2.
    assert abs(snap["lost"] - dev.dropped) <= 3
    assert snap["received"] == pytest.approx(dev.sent, abs=2)
    assert dev.dropped > 0  # the seed must actually exercise loss
    expected_pct = 100.0 * dev.sent / (dev.sent + dev.dropped)
    assert snap["completeness_pct"] == pytest.approx(expected_pct, abs=3.0)
    lat = snap["latency_ms"]
    assert lat is not None and lat["n"] > 30
    # Injected 40 ± 5 ms; localhost ping rtt is tiny so the offset is near-exact.
    assert 34.0 < lat["mean"] < 47.0
    # min is bounded by the injected delay; max only loosely, an OS scheduling
    # hiccup on a busy machine can add tens of ms to a single packet.
    assert lat["min"] >= 33.0 and lat["max"] < 150.0

    with out.open() as f:
        rows = list(csv.reader(f))
    assert rows[0] == LOG_COLUMNS
    assert len(rows) - 1 == snap["received"]


def test_invalid_packets_are_counted():
    rx = TelemetryReceiver(listen_port=0)
    rx.handle_datagram(b"not json", ("127.0.0.1", 1), 0)
    rx.handle_datagram(b'{"v":1,"type":"data","seq":1}', ("127.0.0.1", 1), 0)
    assert rx.snapshot()["invalid_packets"] == 2
