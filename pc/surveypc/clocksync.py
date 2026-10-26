"""Clock offset estimation and sensor-to-PC latency (docs/ICD.md section 3.3).

NTP-style exchange: the PC sends t1 (PC clock), the device stamps t2 on receipt
and t3 before replying (device clock), the PC stamps t4 on receipt.

    rtt    = (t4 - t1) - (t3 - t2)
    offset = ((t2 - t1) + (t3 - t4)) / 2        (device clock - PC clock)

The offset is exact when both network legs take equally long; the error is at
most rtt/2. We therefore use the sample with the smallest rtt in a sliding
window. A data packet stamped t_us (device clock, at sampling time) and received
at pc_rx_us (PC clock) then had

    latency = pc_rx_us - (t_us - offset)
"""

from __future__ import annotations

import math
import time
from collections import deque
from dataclasses import dataclass


def pc_now_us() -> int:
    """Monotonic PC clock in microseconds (immune to wall-clock changes)."""
    return time.monotonic_ns() // 1000


@dataclass(frozen=True)
class SyncSample:
    rtt_us: int
    offset_us: float
    t4_us: int


def sync_sample(t1: int, t2: int, t3: int, t4: int) -> SyncSample:
    rtt = (t4 - t1) - (t3 - t2)
    offset = ((t2 - t1) + (t3 - t4)) / 2.0
    return SyncSample(rtt, offset, t4)


class ClockSync:
    """Keeps the best (lowest-rtt) offset estimate over the last `window` pings."""

    def __init__(self, window: int = 30) -> None:
        self._samples: deque[SyncSample] = deque(maxlen=window)

    def add(self, t1: int, t2: int, t3: int, t4: int) -> SyncSample:
        s = sync_sample(t1, t2, t3, t4)
        if s.rtt_us >= 0:  # negative rtt = corrupted / mismatched pong
            self._samples.append(s)
        return s

    def reset(self) -> None:
        """Call when the device rebooted (its clock restarted)."""
        self._samples.clear()

    @property
    def synced(self) -> bool:
        return bool(self._samples)

    def best(self) -> SyncSample | None:
        return min(self._samples, key=lambda s: s.rtt_us) if self._samples else None

    @property
    def offset_us(self) -> float | None:
        b = self.best()
        return b.offset_us if b else None

    @property
    def uncertainty_us(self) -> float | None:
        b = self.best()
        return b.rtt_us / 2.0 if b else None

    def latency_us(self, device_t_us: int, pc_rx_us: int) -> float | None:
        off = self.offset_us
        return None if off is None else pc_rx_us - (device_t_us - off)


@dataclass
class RunningStats:
    """min / mean / max / std without storing samples (Welford)."""

    n: int = 0
    mean: float = 0.0
    _m2: float = 0.0
    min: float = math.inf
    max: float = -math.inf

    def add(self, x: float) -> None:
        self.n += 1
        d = x - self.mean
        self.mean += d / self.n
        self._m2 += d * (x - self.mean)
        self.min = min(self.min, x)
        self.max = max(self.max, x)

    @property
    def std(self) -> float:
        return math.sqrt(self._m2 / (self.n - 1)) if self.n > 1 else 0.0
