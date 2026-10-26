"""Draws the architecture diagrams for the report (docs/report/img/*.png).

Usage: .venv/bin/python tools/draw_diagrams.py
"""

from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch  # noqa: E402

OUT = Path(__file__).resolve().parents[1] / "docs" / "report" / "img"
C = {"mcu": "#dbe9f6", "sensor": "#e8f4e1", "store": "#fdf0d5", "pc": "#efe3f6", "power": "#f9dede",
     "task": "#dbe9f6", "queue": "#fdf0d5", "ext": "#eeeeee"}


def box(ax, x, y, w, h, text, color, fs=9, bold=False):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.08", fc=color, ec="#333",
                                lw=1.2))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs,
            fontweight="bold" if bold else "normal", wrap=True)
    return (x, y, w, h)


def arrow(ax, a, b, label="", both=False, fs=7.5, rad=0.0, side="a"):
    ax.add_patch(FancyArrowPatch(a, b, arrowstyle="<|-|>" if both else "-|>", mutation_scale=11, lw=1.1,
                                 color="#333", connectionstyle=f"arc3,rad={rad}"))
    if label:
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        ax.text(mx, my + 0.12, label, ha="center", va="bottom", fontsize=fs, color="#222",
                bbox=dict(fc="white", ec="none", pad=0.5))


def canvas(w, h, title):
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, w)
    ax.set_ylim(0, h)
    ax.axis("off")
    ax.set_title(title, fontsize=12, fontweight="bold")
    return fig, ax


def hardware():
    fig, ax = canvas(12, 7, "Hardware block diagram (ESP32 DevKit V1)")
    mcu = box(ax, 4.5, 2.4, 3.0, 2.4, "ESP32-WROOM-32\n240 MHz dual core\nFreeRTOS\nWi-Fi 2.4 GHz", C["mcu"], 10, True)
    gnss = box(ax, 0.3, 5.3, 2.8, 1.1, "u-blox NEO-M8N GNSS\nNMEA 9600 baud", C["sensor"])
    imu = box(ax, 0.3, 3.7, 2.8, 0.9, "MPU6050 IMU\n0x68", C["sensor"])
    env = box(ax, 0.3, 2.5, 2.8, 0.9, "BME280\nT / p / RH  0x76", C["sensor"])
    tof = box(ax, 0.3, 1.3, 2.8, 0.9, "VL53L0X distance\n0x29", C["sensor"])
    sd = box(ax, 8.8, 4.9, 2.9, 1.1, "microSD (SPI)\nFAT32 CSV log", C["store"])
    ui = box(ax, 8.8, 3.3, 2.9, 1.1, "LEDs: fix / log / error\nButton: observation mark", C["ext"])
    pc = box(ax, 8.8, 1.3, 2.9, 1.4, "Laptop (same hotspot)\nreceiver + dashboard\nanalysis + QGIS", C["pc"])
    pwr = box(ax, 4.5, 0.3, 3.0, 1.0, "5 V source: power bank or 18650 + boost\n(not decided) → fuse → switch\nLDO 3.3 V sensor rail", C["power"], 8)
    arrow(ax, (3.1, 5.85), (4.5, 4.5), "UART2  RX16 / TX17", both=True)
    arrow(ax, (3.1, 4.15), (4.5, 3.9), both=True)
    arrow(ax, (3.1, 2.95), (4.5, 3.3), both=True)
    arrow(ax, (3.1, 1.75), (4.5, 2.7), both=True)
    ax.text(3.8, 2.05, "I²C bus  SDA21 / SCL22\n(shared) + XSHUT 33", ha="center", fontsize=7.5,
            bbox=dict(fc="white", ec="none", pad=0.5))
    arrow(ax, (7.5, 4.4), (8.8, 5.45), "SPI  CS5 SCK18 MOSI23 MISO19")
    arrow(ax, (7.5, 3.6), (8.8, 3.85), "GPIO 25/26/27, 32", both=True)
    arrow(ax, (7.5, 2.8), (8.8, 2.0), "Wi-Fi UDP 5005/5006", both=True)
    arrow(ax, (6.0, 1.3), (6.0, 2.4), "5 V / 3.3 V")
    fig.savefig(OUT / "hardware_block_diagram.png", dpi=150, bbox_inches="tight")
    plt.close(fig)


def firmware():
    fig, ax = canvas(13, 7.5, "Firmware architecture: FreeRTOS tasks and data flow")
    hal = box(ax, 0.3, 0.3, 12.4, 0.8, "HAL interfaces (lib/hal):  Clock · GnssSerial · Imu · EnvSensor · RangeSensor · "
              "Storage      →  real drivers (USE_MOCK=0)  |  mock devices (USE_MOCK=1)", C["ext"], 9)
    g = box(ax, 0.4, 4.6, 2.6, 1.3, "gnssTask  (prio 4, 20 ms)\nUART → NMEA parser", C["task"])
    s = box(ax, 0.4, 2.3, 2.6, 1.5, "sensorTask (prio 4, 20 ms)\nToF 50 Hz · IMU 10 Hz\nBME 1 Hz · Supervisor", C["task"])
    sh = box(ax, 3.8, 3.3, 2.4, 1.8, "SharedState\n(mutex)\nGNSS snapshot\nTimeBase\nsensor means", C["queue"])
    sm = box(ax, 7.0, 3.3, 2.6, 1.8, "sampleTask (prio 5)\nvTaskDelayUntil period\nseq · t_us · UTC\nUTM · flags", C["task"], 9, True)
    lq = box(ax, 10.2, 5.0, 1.0, 0.8, "logQueue", C["queue"], 8)
    nq = box(ax, 10.2, 2.6, 1.0, 0.8, "netQueue", C["queue"], 8)
    lg = box(ax, 11.5, 4.8, 1.4, 1.2, "logTask\nbuffered\nSD CSV", C["task"], 8)
    nt = box(ax, 11.5, 2.4, 1.4, 1.2, "netTask\nUDP JSON\nhb · pong", C["task"], 8)
    ui = box(ax, 3.8, 6.2, 2.4, 0.9, "uiTask: LEDs, button → mark", C["task"], 8)
    mon = box(ax, 7.0, 6.2, 2.6, 0.9, "monitorTask: # STAT report\nconsoleTask: Wi-Fi commands", C["task"], 8)
    arrow(ax, (3.0, 5.25), (3.8, 4.6), "fix, UTC")
    arrow(ax, (3.0, 3.05), (3.8, 3.8), "samples")
    arrow(ax, (6.2, 4.2), (7.0, 4.2), "copy")
    arrow(ax, (9.6, 4.6), (10.2, 5.4), "")
    arrow(ax, (9.6, 3.8), (10.2, 3.0), "")
    arrow(ax, (11.2, 5.4), (11.5, 5.4), "")
    arrow(ax, (11.2, 3.0), (11.5, 3.0), "")
    arrow(ax, (5.0, 6.2), (8.0, 5.1), "mark", rad=-0.2)
    ax.text(9.9, 2.1, "queue sends use zero timeout\n(full queue → drop counted)", fontsize=7, ha="center", style="italic")
    ax.text(6.5, 1.5, "every task reaches hardware only through the HAL  ·  core 1: acquisition  ·  core 0: Wi-Fi + netTask",
            fontsize=8, ha="center", style="italic")
    fig.savefig(OUT / "firmware_architecture.png", dpi=150, bbox_inches="tight")
    plt.close(fig)


def pipeline():
    fig, ax = canvas(13, 3.4, "End-to-end data chain (handout §12 minimum passing prototype)")
    steps = [("Power ON\ninit + supervisor", C["power"]), ("GNSS\nNMEA → fix, UTC", C["sensor"]),
             ("Multi-sensor\nIMU · BME · ToF", C["sensor"]), ("Timestamp\nesp_timer +\nTimeBase", C["task"]),
             ("UTM\nKrüger series", C["task"]), ("SD log\nCSV (ICD)", C["store"]),
             ("Wi-Fi telemetry\nUDP JSON", C["pc"]), ("Dashboard\nPySide6", C["pc"]), ("GIS map\nGeoJSON → QGIS", C["pc"])]
    w, gap, y = 1.25, 0.15, 1.5
    for i, (t, c) in enumerate(steps):
        x = 0.25 + i * (w + gap)
        box(ax, x, y, w, 1.1, t, c, 7.5)
        if i:
            arrow(ax, (x - gap, y + 0.55), (x, y + 0.55))
    ax.text(6.5, 0.95, "Verification:  native unit tests (firmware lib/)  ·  pytest (PC)  ·  golden files shared by both  ·  "
            "10-min pipeline simulation  ·  UTM vs pyproj < 0.001 mm", ha="center", fontsize=8)
    ax.text(6.5, 0.5, "Analysis: analysis/run_all.py → GNSS error, UTM check, repeatability, sampling, latency, "
            "completeness, route comparison", ha="center", fontsize=8)
    fig.savefig(OUT / "data_pipeline.png", dpi=150, bbox_inches="tight")
    plt.close(fig)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    hardware()
    firmware()
    pipeline()
    print("diagrams written to", OUT)
