"""Virtual ESP32: sends the same UDP packets as the real firmware, without hardware.

Replays a survey CSV (default: data/sample/mock_survey.csv from the firmware
pipeline simulation) as live telemetry: data packets at the chosen rate,
heartbeats at 1 Hz, and pong replies to pings, with its own "device clock".
Optional loss / delay / jitter make the receiver's statistics testable.

Run:  .venv/bin/python -m surveypc.virtual_esp32 --loss 0.05 --delay-ms 25
"""

from __future__ import annotations

import argparse
import signal
import heapq
import itertools
import json
import random
import socket
import threading
import time
from pathlib import Path

from .clocksync import pc_now_us
from .protocol import FLAG_WIFI, PROTOCOL_VERSION, encode_data_json, parse_csv_line

DEFAULT_SOURCE = Path(__file__).resolve().parents[2] / "data" / "sample" / "mock_survey.csv"


class VirtualEsp32:
    def __init__(
        self,
        source: Path = DEFAULT_SOURCE,
        pc_host: str = "127.0.0.1",
        pc_port: int = 5005,
        device_port: int = 0,
        rate_hz: float = 1.0,
        loss: float = 0.0,
        delay_ms: float = 0.0,
        jitter_ms: float = 0.0,
        seed: int = 1,
        loop: bool = True,
    ) -> None:
        lines = Path(source).read_text().splitlines()[1:]
        self.rows = [parse_csv_line(line) for line in lines if line.strip()]
        self.pc = (pc_host, pc_port)
        self.rate_hz = rate_hz
        self.loss = loss
        self.delay_us = delay_ms * 1000.0
        self.jitter_us = jitter_ms * 1000.0
        self.loop = loop
        self.rng = random.Random(seed)
        # Device clock = µs since "boot"; deliberately unrelated to the PC clock.
        self._boot_pc_us = pc_now_us() - self.rng.randint(5_000_000, 50_000_000)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind(("0.0.0.0", device_port))
        self.sock.settimeout(0.05)
        self.device_port = self.sock.getsockname()[1]
        self.sent = 0
        self.dropped = 0
        self.pongs = 0
        self._outbox: list[tuple[int, int, bytes]] = []  # (send_at_pc_us, tiebreak, payload)
        self._counter = itertools.count()
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._threads: list[threading.Thread] = []

    def device_now_us(self) -> int:
        return pc_now_us() - self._boot_pc_us

    # ------------------------------------------------------------ lifecycle

    def start(self) -> None:
        for target in (self._produce_loop, self._send_loop, self._ping_loop):
            t = threading.Thread(target=target, daemon=True)
            t.start()
            self._threads.append(t)

    def stop(self) -> None:
        self._stop.set()
        for t in self._threads:
            t.join(timeout=2)
        self.sock.close()

    def __enter__(self) -> "VirtualEsp32":
        self.start()
        return self

    def __exit__(self, *exc) -> None:
        self.stop()

    # ------------------------------------------------------------ behaviour

    def _queue(self, payload: str, delayed: bool) -> None:
        delay = 0.0
        if delayed:
            delay = max(0.0, self.delay_us + self.rng.uniform(-self.jitter_us, self.jitter_us))
        with self._lock:
            heapq.heappush(self._outbox, (pc_now_us() + int(delay), next(self._counter), payload.encode()))

    def _produce_loop(self) -> None:
        period = 1.0 / self.rate_hz
        next_t = time.monotonic()
        last_hb = 0.0
        for seq in itertools.count():
            if self._stop.is_set() or (not self.loop and seq >= len(self.rows)):
                return
            rec = dict(self.rows[seq % len(self.rows)])
            rec["seq"] = seq
            rec["t_us"] = self.device_now_us()  # sampling time on the device clock
            rec["flags"] = (rec["flags"] or 0) | FLAG_WIFI
            if self.rng.random() < self.loss:
                self.dropped += 1
            else:
                self._queue(encode_data_json(rec), delayed=True)
                self.sent += 1
            now = time.monotonic()
            if now - last_hb >= 1.0:
                last_hb = now
                hb = {"v": PROTOCOL_VERSION, "type": "hb", "t_us": self.device_now_us(),
                      "uptime_s": self.device_now_us() // 1_000_000, "heap": 180000, "rssi": -55,
                      "sd_free_mb": 7400, "q_drop": 0, "sd_err": 0, "net_drop": self.dropped, "fw": "virtual"}
                self._queue(json.dumps(hb, separators=(",", ":")), delayed=False)
            next_t += period
            self._stop.wait(max(0.0, next_t - time.monotonic()))

    def _send_loop(self) -> None:
        while not self._stop.is_set():
            with self._lock:
                due = self._outbox and self._outbox[0][0] <= pc_now_us()
                item = heapq.heappop(self._outbox) if due else None
            if item is None:
                time.sleep(0.001)
                continue
            try:
                self.sock.sendto(item[2], self.pc)
            except OSError:
                pass

    def _ping_loop(self) -> None:
        while not self._stop.is_set():
            try:
                payload, addr = self.sock.recvfrom(512)
            except socket.timeout:
                continue
            except OSError:
                return
            t2 = self.device_now_us()
            try:
                ping = json.loads(payload)
            except json.JSONDecodeError:
                continue
            if ping.get("type") != "ping":
                continue
            pong = {"v": PROTOCOL_VERSION, "type": "pong", "id": ping["id"], "t1": ping["t1"], "t2": t2,
                    "t3": self.device_now_us()}
            self.sock.sendto(json.dumps(pong, separators=(",", ":")).encode(), addr)
            self.pongs += 1


def _raise_interrupt(*_):
    raise KeyboardInterrupt


def main() -> None:
    signal.signal(signal.SIGTERM, _raise_interrupt)  # stop cleanly on kill, like Ctrl+C
    ap = argparse.ArgumentParser(description="Virtual ESP32 telemetry source")
    ap.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    ap.add_argument("--host", default="127.0.0.1", help="PC address (255.255.255.255 = broadcast)")
    ap.add_argument("--port", type=int, default=5005)
    ap.add_argument("--rate", type=float, default=1.0, help="records per second")
    ap.add_argument("--loss", type=float, default=0.0, help="drop probability per data packet")
    ap.add_argument("--delay-ms", type=float, default=0.0)
    ap.add_argument("--jitter-ms", type=float, default=0.0)
    args = ap.parse_args()
    with VirtualEsp32(args.source, args.host, args.port, rate_hz=args.rate, loss=args.loss,
                      delay_ms=args.delay_ms, jitter_ms=args.jitter_ms) as dev:
        print(f"virtual ESP32 on UDP {dev.device_port} -> {args.host}:{args.port}, "
              f"{len(dev.rows)} records, {args.rate} Hz (Ctrl+C to stop)")
        try:
            while True:
                time.sleep(5)
                print(f"sent={dev.sent} dropped={dev.dropped} pongs={dev.pongs}")
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
