"""Live monitoring dashboard (requirement R8): connection, position, UTM,
sensors and GNSS quality, plus the survey track and time plots.

Run with the real device:     .venv/bin/python -m surveypc.dashboard
Run without hardware (demo):  .venv/bin/python -m surveypc.dashboard --demo

The UDP receiver runs on its own thread; records reach the GUI through a
thread-safe queue that a 5 Hz timer drains, so the window never blocks on the
network.
"""

from __future__ import annotations

import argparse
import queue
import sys
import time
from pathlib import Path

import pyqtgraph as pg
from PySide6 import QtCore, QtGui, QtWidgets

from . import dashboard_model as m
from .receiver import PC_PORT, TelemetryReceiver

COLORS = {m.OK: "#1b7f3b", m.WARN: "#b36b00", m.BAD: "#c0392b", m.NONE: "#555555"}
REFRESH_MS = 200


def styled(label: QtWidgets.QLabel, f: m.Field) -> None:
    label.setText(f.text)
    label.setStyleSheet(f"color: {COLORS[f.level]}; font-weight: {'600' if f.level != m.NONE else '400'};")


class FieldGroup(QtWidgets.QGroupBox):
    """A titled box of 'name: value' rows."""

    def __init__(self, title: str, rows: list[tuple[str, str]]) -> None:
        super().__init__(title)
        form = QtWidgets.QFormLayout(self)
        form.setLabelAlignment(QtCore.Qt.AlignRight)
        self.labels: dict[str, QtWidgets.QLabel] = {}
        for key, caption in rows:
            lab = QtWidgets.QLabel("—")
            lab.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
            form.addRow(caption + ":", lab)
            self.labels[key] = lab

    def update_fields(self, fields: dict[str, m.Field]) -> None:
        for key, f in fields.items():
            if key in self.labels:
                styled(self.labels[key], f)


class Dashboard(QtWidgets.QMainWindow):
    def __init__(self, receiver: TelemetryReceiver) -> None:
        super().__init__()
        self.rx = receiver
        self.model = m.DashboardModel()
        self.inbox: queue.SimpleQueue = queue.SimpleQueue()
        receiver.on_record = lambda rec, lat: self.inbox.put((rec, lat))
        self.setWindowTitle("ENG401 Survey Dashboard")
        self.resize(1400, 860)
        self._build()
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.refresh)
        self.timer.start(REFRESH_MS)

    # ------------------------------------------------------------ layout

    def _build(self) -> None:
        central = QtWidgets.QWidget()
        root = QtWidgets.QVBoxLayout(central)

        top = QtWidgets.QHBoxLayout()
        self.conn = QtWidgets.QLabel()
        self.conn.setFont(QtGui.QFont("", 15, QtGui.QFont.Bold))
        self.complete = QtWidgets.QLabel()
        self.latency = QtWidgets.QLabel()
        top.addWidget(self.conn, 2)
        top.addWidget(QtWidgets.QLabel("Completeness:"))
        top.addWidget(self.complete, 2)
        top.addWidget(QtWidgets.QLabel("Latency:"))
        top.addWidget(self.latency, 3)
        root.addLayout(top)

        body = QtWidgets.QHBoxLayout()
        left = QtWidgets.QVBoxLayout()
        self.gnss = FieldGroup("GNSS", [("fix", "Fix"), ("sats", "Satellites"), ("hdop", "HDOP"), ("utc", "UTC"),
                                        ("position", "Lat, Lon"), ("utm", "UTM"), ("alt", "Altitude"),
                                        ("speed", "Speed")])
        self.sensors = FieldGroup("Sensors", [("temp", "Temperature"), ("pres", "Pressure"), ("hum", "Humidity"),
                                              ("dist", "Distance"), ("accel", "Accel x y z"), ("gyro", "Gyro x y z")])
        self.flags = FieldGroup("Status", [(name, name) for name, _ in m.SENSOR_FLAGS] + [("Mode", "Mode")])
        self.device = FieldGroup("Device", [("seq", "Record #"), ("uptime", "Uptime"), ("rssi", "Wi-Fi RSSI"),
                                            ("sd", "SD free"), ("drops", "Drops q/sd/net"), ("fw", "Firmware")])
        for g in (self.gnss, self.sensors, self.flags, self.device):
            left.addWidget(g)
        left.addStretch(1)
        left_w = QtWidgets.QWidget()
        left_w.setLayout(left)
        left_w.setFixedWidth(430)
        body.addWidget(left_w)

        pg.setConfigOptions(antialias=True, background="w", foreground="k")
        plots = pg.GraphicsLayoutWidget()
        self.track = plots.addPlot(row=0, col=0, rowspan=3, title="Track (UTM, relative)")
        self.track.setAspectLocked(True)
        self.track.showGrid(x=True, y=True, alpha=0.3)
        self.track.setLabel("bottom", "Easting", "m")
        self.track.setLabel("left", "Northing", "m")
        self.track_line = self.track.plot(pen=pg.mkPen("#1f77b4", width=2))
        self.track_head = self.track.plot(symbol="o", symbolSize=11, symbolBrush="#d62728", pen=None)
        self.track_marks = self.track.plot(symbol="t", symbolSize=13, symbolBrush="#2ca02c", pen=None)
        self.mark_labels: list[pg.TextItem] = []

        def small(row: int, title: str, unit: str) -> pg.PlotItem:
            pl = plots.addPlot(row=row, col=1, title=title)
            pl.showGrid(x=True, y=True, alpha=0.3)
            pl.setLabel("left", unit)
            return pl

        self.p_env = small(0, "Temperature / humidity", "")
        self.c_temp = self.p_env.plot(pen=pg.mkPen("#d62728", width=2), name="°C")
        self.c_hum = self.p_env.plot(pen=pg.mkPen("#1f77b4", width=2), name="%RH")
        self.p_env.addLegend(offset=(5, 5))
        self.p_env.legend.addItem(self.c_temp, "temp °C")
        self.p_env.legend.addItem(self.c_hum, "hum %RH")
        self.p_dist = small(1, "Distance", "mm")
        self.c_dist = self.p_dist.plot(pen=pg.mkPen("#9467bd", width=2))
        self.p_q = small(2, "GNSS quality (sats, HDOP)", "")
        self.c_sats = self.p_q.plot(pen=pg.mkPen("#2ca02c", width=2))
        self.c_hdop = self.p_q.plot(pen=pg.mkPen("#ff7f0e", width=2))
        self.p_q.addLegend(offset=(5, 5))
        self.p_q.legend.addItem(self.c_sats, "satellites")
        self.p_q.legend.addItem(self.c_hdop, "HDOP")
        self.p_lat = small(3, "Sensor-to-PC latency", "ms")
        self.c_lat = self.p_lat.plot(pen=pg.mkPen("#17becf", width=2))
        self.p_az = plots.addPlot(row=3, col=0, title="Vertical acceleration")
        self.p_az.showGrid(x=True, y=True, alpha=0.3)
        self.p_az.setLabel("left", "m/s²")
        self.c_az = self.p_az.plot(pen=pg.mkPen("#7f7f7f", width=1))
        for pl in (self.p_dist, self.p_q, self.p_lat, self.p_az):
            pl.setXLink(self.p_env)
        body.addWidget(plots, 1)
        root.addLayout(body, 1)
        self.setCentralWidget(central)

    # ------------------------------------------------------------ refresh

    def refresh(self) -> None:
        got = False
        while True:
            try:
                rec, lat = self.inbox.get_nowait()
            except queue.Empty:
                break
            self.model.add(rec, lat)
            got = True

        snap = self.rx.snapshot()
        styled(self.conn, m.connection_field(snap))
        styled(self.complete, m.completeness_field(snap))
        styled(self.latency, m.latency_field(snap))
        rec = snap["record"]
        self.gnss.update_fields(m.gnss_fields(rec))
        self.sensors.update_fields(m.sensor_fields(rec))
        self.flags.update_fields(dict(m.flag_fields(rec)))
        hb = snap["heartbeat"]
        self.device.update_fields({
            "seq": m.Field(str(rec["seq"]) if rec else "—"),
            "uptime": m.Field(f"{hb['uptime_s']} s" if hb else "—"),
            "rssi": m.Field(f"{hb['rssi']} dBm" if hb else "—",
                            m.NONE if not hb else m.OK if hb["rssi"] > -70 else m.WARN),
            "sd": m.Field(f"{hb['sd_free_mb']} MB" if hb else "—"),
            "drops": m.Field(f"{hb.get('q_drop', 0)} / {hb.get('sd_err', 0)} / {hb.get('net_drop', 0)}" if hb else "—"),
            "fw": m.Field(str(hb.get("fw")) if hb else "—"),
        })
        if got:
            self._update_plots()

    def _update_plots(self) -> None:
        md = self.model
        if md.zone:
            self.track.setTitle(f"Track — UTM zone {md.zone}, origin E {md.origin[0]:.0f} N {md.origin[1]:.0f}")
        self.track_line.setData(md.track_e, md.track_n)
        if md.track_e:
            self.track_head.setData([md.track_e[-1]], [md.track_n[-1]])
        if len(md.marks) != len(self.mark_labels):
            self.track_marks.setData([e for _, e, _ in md.marks], [n for _, _, n in md.marks])
            for mark_id, e, n in md.marks[len(self.mark_labels):]:
                t = pg.TextItem(f"P{mark_id}", color="#2ca02c", anchor=(0, 1))
                t.setPos(e, n)
                self.track.addItem(t)
                self.mark_labels.append(t)
        t = list(md.t)
        s = md.series
        self.c_temp.setData(t, list(s["temp"]), connect="finite")
        self.c_hum.setData(t, list(s["hum"]), connect="finite")
        self.c_dist.setData(t, list(s["dist"]), connect="finite")
        self.c_sats.setData(t, list(s["sats"]), connect="finite")
        self.c_hdop.setData(t, list(s["hdop"]), connect="finite")
        self.c_lat.setData(t, list(s["latency"]), connect="finite")
        self.c_az.setData(t, list(s["az"]), connect="finite")


def main() -> None:
    ap = argparse.ArgumentParser(description="ENG401 live dashboard")
    ap.add_argument("--port", type=int, default=PC_PORT)
    ap.add_argument("--out", type=Path, default=Path(f"data/processed/telemetry_{time.strftime('%Y%m%d_%H%M%S')}.csv"),
                    help="telemetry CSV log")
    ap.add_argument("--demo", action="store_true", help="start a virtual ESP32 (no hardware needed)")
    ap.add_argument("--demo-rate", type=float, default=2.0)
    ap.add_argument("--screenshot", type=Path, help="save a PNG after --after seconds and exit")
    ap.add_argument("--after", type=float, default=10.0)
    args = ap.parse_args()

    app = QtWidgets.QApplication(sys.argv)
    rx = TelemetryReceiver(args.port, args.out)
    win = Dashboard(rx)
    win.show()
    devices = []

    def start_telemetry() -> None:
        # Start only once the window is up: GUI start-up holds the Python
        # interpreter for seconds and would show up as fake network latency.
        rx.start()
        if args.demo:
            from .virtual_esp32 import VirtualEsp32

            dev = VirtualEsp32(pc_port=rx.listen_port, rate_hz=args.demo_rate, delay_ms=20, jitter_ms=8, loss=0.02)
            dev.start()
            devices.append(dev)

    QtCore.QTimer.singleShot(0, start_telemetry)
    if args.screenshot:
        def shoot() -> None:
            win.refresh()
            win.grab().save(str(args.screenshot))
            app.quit()

        QtCore.QTimer.singleShot(int(args.after * 1000), shoot)
    code = app.exec()
    for dev in devices:
        dev.stop()
    rx.stop()
    sys.exit(code)


if __name__ == "__main__":
    main()
