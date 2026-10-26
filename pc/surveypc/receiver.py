"""UDP telemetry receiver (PC side of docs/ICD.md section 3).

- Listens for data / hb / pong datagrams on UDP 5005.
- Learns the device address from the first packet and pings it every second
  (NTP-style) to estimate the clock offset -> sensor-to-PC latency per record.
- Tracks packet loss from sequence numbers and logs every record to CSV
  (ICD section 4: all record columns + pc_rx_us + latency_ms).

Run:  .venv/bin/python -m surveypc.receiver --out data/processed/telemetry.csv
"""

from __future__ import annotations

import argparse
import signal
import csv
import json
import socket
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

from .clocksync import ClockSync, RunningStats, pc_now_us
from .protocol import COLUMNS, PROTOCOL_VERSION, ProtocolError, Record, decode_packet
from .seqtrack import SeqTracker

PC_PORT = 5005
LOG_COLUMNS = COLUMNS + ["pc_rx_us", "latency_ms"]


@dataclass
class ReceiverState:
    device_addr: tuple[str, int] | None = None
    last_rx_us: int | None = None
    last_record: Record | None = None
    last_hb: Record | None = None
    invalid_packets: int = 0
    pings_sent: int = 0
    pongs: int = 0
    seq: SeqTracker = field(default_factory=SeqTracker)
    latency_ms: RunningStats = field(default_factory=RunningStats)
    last_latency_ms: float | None = None


class TelemetryReceiver:
    def __init__(
        self,
        listen_port: int = PC_PORT,
        out_csv: Path | None = None,
        ping_interval_s: float = 1.0,
        on_record: Callable[[Record, float | None], None] | None = None,
        bind_host: str = "0.0.0.0",
    ) -> None:
        self.listen_port = listen_port
        self.out_csv = out_csv
        self.ping_interval_s = ping_interval_s
        self.on_record = on_record
        self.bind_host = bind_host
        self.sync = ClockSync()
        self.state = ReceiverState()
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._threads: list[threading.Thread] = []
        self._sock: socket.socket | None = None
        self._csv_file = None
        self._csv: csv.writer | None = None
        self._ping_id = 0

    # ------------------------------------------------------------ lifecycle

    def start(self) -> None:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((self.bind_host, self.listen_port))
        s.settimeout(0.2)
        self._sock = s
        self.listen_port = s.getsockname()[1]
        if self.out_csv:
            Path(self.out_csv).parent.mkdir(parents=True, exist_ok=True)
            self._csv_file = open(self.out_csv, "w", newline="", buffering=1)
            self._csv = csv.writer(self._csv_file, lineterminator="\n")
            self._csv.writerow(LOG_COLUMNS)
        for target in (self._rx_loop, self._ping_loop):
            t = threading.Thread(target=target, daemon=True)
            t.start()
            self._threads.append(t)

    def stop(self) -> None:
        self._stop.set()
        for t in self._threads:
            t.join(timeout=2)
        if self._sock:
            self._sock.close()
        if self._csv_file:
            self._csv_file.close()

    def __enter__(self) -> "TelemetryReceiver":
        self.start()
        return self

    def __exit__(self, *exc) -> None:
        self.stop()

    # ------------------------------------------------------------ threads

    def _rx_loop(self) -> None:
        assert self._sock is not None
        while not self._stop.is_set():
            try:
                payload, addr = self._sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                break
            self.handle_datagram(payload, addr, pc_now_us())

    def _ping_loop(self) -> None:
        while not self._stop.wait(self.ping_interval_s):
            with self._lock:
                addr = self.state.device_addr
            if addr is None or self._sock is None:
                continue
            self._ping_id += 1
            msg = json.dumps({"v": PROTOCOL_VERSION, "type": "ping", "id": self._ping_id, "t1": pc_now_us()})
            try:
                self._sock.sendto(msg.encode(), addr)
                with self._lock:
                    self.state.pings_sent += 1
            except OSError:
                pass

    # ------------------------------------------------------------ packet handling

    def handle_datagram(self, payload: bytes, addr: tuple[str, int], rx_us: int) -> None:
        try:
            pkt = decode_packet(payload)
        except ProtocolError:
            with self._lock:
                self.state.invalid_packets += 1
            return
        with self._lock:
            st = self.state
            st.device_addr = addr
            st.last_rx_us = rx_us
            kind = pkt["type"]
            if kind == "pong":
                self.sync.add(int(pkt["t1"]), int(pkt["t2"]), int(pkt["t3"]), rx_us)
                st.pongs += 1
                return
            if kind == "hb":
                st.last_hb = pkt
                return
            if kind != "data":
                return
            rec = {c: pkt[c] for c in COLUMNS}
            if st.seq.add(rec["seq"]) == "reboot":
                self.sync.reset()  # device clock restarted
            lat_us = self.sync.latency_us(rec["t_us"], rx_us)
            lat_ms = None if lat_us is None else lat_us / 1000.0
            if lat_ms is not None:
                st.latency_ms.add(lat_ms)
            st.last_latency_ms = lat_ms
            st.last_record = rec
            if self._csv:
                row = ["" if rec[c] is None else rec[c] for c in COLUMNS]
                self._csv.writerow(row + [rx_us, "" if lat_ms is None else f"{lat_ms:.3f}"])
        if self.on_record:
            self.on_record(rec, lat_ms)  # called on the receiver thread

    # ------------------------------------------------------------ status

    def snapshot(self) -> dict:
        """Thread-safe copy of the receiver status (for dashboard / console)."""
        with self._lock:
            st = self.state
            age = None if st.last_rx_us is None else (pc_now_us() - st.last_rx_us) / 1e6
            lat = st.latency_ms
            return {
                "device": st.device_addr,
                "seconds_since_rx": age,
                "connected": age is not None and age < 3.0,
                "record": dict(st.last_record) if st.last_record else None,
                "heartbeat": dict(st.last_hb) if st.last_hb else None,
                "received": st.seq.received,
                "lost": st.seq.lost,
                "duplicates": st.seq.duplicates,
                "reboots": st.seq.reboots,
                "completeness_pct": st.seq.completeness_pct,
                "invalid_packets": st.invalid_packets,
                "latency_ms": {"n": lat.n, "min": lat.min, "mean": lat.mean, "max": lat.max, "std": lat.std,
                               "last": st.last_latency_ms} if lat.n else None,
                "clock_offset_us": self.sync.offset_us,
                "sync_uncertainty_ms": None if self.sync.uncertainty_us is None
                else self.sync.uncertainty_us / 1000.0,
                "pings": st.pings_sent,
                "pongs": st.pongs,
            }


def format_status(s: dict) -> str:
    lat = s["latency_ms"]
    lat_txt = (f"latency ms min/mean/max {lat['min']:.1f}/{lat['mean']:.1f}/{lat['max']:.1f} (n={lat['n']})"
               if lat else "latency: waiting for clock sync")
    unc = s["sync_uncertainty_ms"]
    return (f"{'CONNECTED' if s['connected'] else 'NO DATA':9} dev={s['device']} rx={s['received']} "
            f"lost={s['lost']} complete={s['completeness_pct']:.1f}% {lat_txt}"
            + (f" ±{unc:.1f}" if unc is not None else ""))


def _raise_interrupt(*_):
    raise KeyboardInterrupt


def main() -> None:
    signal.signal(signal.SIGTERM, _raise_interrupt)  # stop cleanly on kill, like Ctrl+C
    ap = argparse.ArgumentParser(description="ENG401 telemetry receiver")
    ap.add_argument("--port", type=int, default=PC_PORT)
    ap.add_argument("--out", type=Path, default=Path(f"data/processed/telemetry_{time.strftime('%Y%m%d_%H%M%S')}.csv"))
    ap.add_argument("--interval", type=float, default=5.0, help="status print interval (s)")
    args = ap.parse_args()
    with TelemetryReceiver(args.port, args.out) as rx:
        print(f"listening on UDP {rx.listen_port}, logging to {args.out} (Ctrl+C to stop)")
        try:
            while True:
                time.sleep(args.interval)
                print(format_status(rx.snapshot()))
        except KeyboardInterrupt:
            pass
        print("final:", format_status(rx.snapshot()))


if __name__ == "__main__":
    main()
