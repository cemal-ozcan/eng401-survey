"""Packet loss accounting from record sequence numbers."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class SeqTracker:
    """Counts received, lost, duplicate and out-of-order packets.

    A sequence number far below the highest seen (or 0 after >0) is taken as a
    device reboot: counting restarts and `reboots` increments.
    """

    received: int = 0
    lost: int = 0
    duplicates: int = 0
    out_of_order: int = 0
    reboots: int = 0
    first_seq: int | None = None
    highest: int | None = None
    reboot_threshold: int = 100

    def add(self, seq: int) -> str:
        """Returns 'ok', 'gap', 'dup', 'late' or 'reboot'."""
        if self.highest is None:
            self.first_seq = self.highest = seq
            self.received += 1
            self._seen = {seq}
            return "ok"
        if seq < self.highest and (seq == 0 or self.highest - seq > self.reboot_threshold):
            self.reboots += 1
            self.first_seq = self.highest = seq
            self.received += 1
            self._seen = {seq}
            return "reboot"
        if seq in self._seen:
            self.duplicates += 1
            return "dup"
        self._seen.add(seq)
        self.received += 1
        if seq < self.highest:
            # counted as lost when the gap opened; it arrived after all
            self.out_of_order += 1
            self.lost -= 1
            return "late"
        result = "ok"
        if seq > self.highest + 1:
            self.lost += seq - self.highest - 1
            result = "gap"
        self.highest = seq
        if len(self._seen) > 4 * self.reboot_threshold:  # bound memory
            self._seen = {s for s in self._seen if s > self.highest - 2 * self.reboot_threshold}
        return result

    @property
    def expected(self) -> int:
        return self.received + self.lost

    @property
    def completeness_pct(self) -> float:
        return 100.0 * self.received / self.expected if self.expected else 0.0
