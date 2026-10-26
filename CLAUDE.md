# ENG401 — Portable Smart Surveying & Environmental Mapping System

## Purpose
Hand-carried survey device: ESP32 + GNSS + IMU + BME280 + VL53L0X + microSD.
Chain that MUST work end-to-end at the final demo (handout §12):
`Power ON → GNSS → multi-sensor acquisition → timestamping → UTM → SD logging → Wi-Fi telemetry → PC dashboard → GIS map`

Binding sources (in `reference/`): `eng401_handout.pdf` (R1–R11, weekly milestones, D1–D8,
required tests, grading), `ENG401_Satin_Alinacak_Malzemeler.docx` (parts list = hardware baseline; pending instructor approval).
`old_role_table.jpeg` is historical only.

## Team & division of work
- 12 students: 6 Computer Eng. (CE) + 6 Geomatics Eng. (GE).
- **Software owner (remote, NO hardware access):** all firmware, drivers, FreeRTOS, timestamping,
  SD logging, telemetry, PC receiver, dashboard, UTM implementation, analysis/validation scripts.
- **CE hardware team ("hands"):** soldering, wiring, power board, enclosure/pole; flash firmware,
  send back serial logs and test results; field measurements.
- **GE team (after CE part is done):** route design, reference points, independent UTM check,
  GNSS accuracy analysis, GIS maps (QGIS). Software owner supplies the Python scripts they run.
- Communication with the user: Turkish. Code, comments, commit messages: English.
  Everything under `docs/hardware-team/` is Turkish, step-by-step, for non-experts.

## Architecture
```
            ┌──────────── ESP32 (FreeRTOS, Arduino-ESP32) ─────────────┐
 NEO GNSS ─UART2─► gnssTask ──┐                                         │
 MPU6050 ─┐                   ├─► sampleTask (fixed period, seq#, t_us) │
 BME280  ─┼─I²C─► sensorTask ─┘        │  builds Record, UTM convert    │
 VL53L0X ─┘                            ├─► Queue ─► logTask ─► SD (buffered CSV)
                                       └─► Queue ─► netTask ─► UDP ───────┼──► PC
 LEDs/button ◄─► uiTask                                                   │
            └─────────────────────────────────────────────────────────────┘
 PC: receiver (UDP) → CSV + dashboard (PyQt/pyqtgraph) ; analysis scripts → GeoJSON/CSV → QGIS
```
- **Time base:** `esp_timer_get_time()` (µs, monotonic) stamps every sample. GNSS RMC/ZDA UTC
  disciplines an offset → `utc = t_us + offset`. No PPS assumed (see unknowns).
- **UTM:** computed on-device with our own Transverse Mercator (Krüger series, WGS84);
  PC re-derives with `pyproj` as independent verification (handout §9).
- **Telemetry:** UDP JSON (see `docs/ICD.md`). Latency via NTP-style ping/pong clock-offset estimate.
- **Dashboard:** PyQt + pyqtgraph (live streaming), receiver thread separate from UI.

## Folder layout
```
firmware/           PlatformIO project (env: esp32 real, esp32-mock, diag, native tests)
  src/app/ src/diag/  survey application / diagnostic firmware (selected by build_src_filter)
  include/ src/     app code; lib/ = our portable libs (nmea, utm, record, timebase) – no Arduino deps
  test/             PlatformIO native (host) unit tests for lib/
wokwi/              Wokwi simulator config (diagram.json, wokwi.toml)
pc/surveypc/        receiver, dashboard, virtual_esp32, replay, protocol codec
pc/tests/           pytest
analysis/           required-test scripts (GNSS error, repeatability, timing, latency, completeness, route) + GIS export
data/raw|processed|replay   field logs (large files not committed)
docs/ICD.md         telemetry + CSV data dictionary (single source of truth for formats)
docs/PINOUT.md      pin table (sent to hardware team)
docs/hardware-team/ Turkish step-by-step guides: flashing, wiring, test procedures
shared/golden/       cross-language test fixtures (UTM vectors from pyproj, NMEA sample, record CSV/JSON)
tools/              release script, golden-vector generator
reference/          handout, parts list
```

## Rules
1. **Mock vs real:** every sensor + SD sits behind an interface in `firmware/include/hal/`.
   Build flag `USE_MOCK=1` selects mock implementations (env `esp32-mock`); nothing outside
   `hal/` may include a sensor library. Mocks must model noise, GNSS fix loss, HDOP drift,
   sensor init failure / read errors.
2. **Non-blocking:** no `delay()` in app tasks; use `vTaskDelayUntil`, queues, timeouts.
   No unbounded waits on I²C/UART/SD.
3. Pure logic (NMEA parser, UTM, record/CSV/JSON encoding, timebase) lives in `firmware/lib/`
   with no Arduino includes, so it runs under `pio test -e native`.
4. Formats change only together with `docs/ICD.md`, `shared/golden/*`, firmware `lib/record` and `pc/surveypc/protocol.py` (both are tested byte-exact against the golden files).
5. A step is done only when `pio run` (all envs) and tests (`pio test -e native`, `pytest`) pass.
6. Never invent hardware details. Unverified assumptions go into the Unknowns list below.
7. Releases for the hardware team are git-tagged (`v0.1-diag`, …) with a prebuilt `.bin`.
8. After each milestone, explain the code to the user (they must defend it orally).

## Commands
Tooling lives in the repo venv `.venv/` (PlatformIO, pytest, pyproj). Setup:
`python3 -m venv .venv && .venv/bin/pip install platformio && .venv/bin/pip install -e "pc[dev]"`
- Firmware: `cd firmware && ../.venv/bin/pio run` (builds esp32, esp32-mock, diag) | `../.venv/bin/pio test -e native`
- PC: `cd pc && ../.venv/bin/pytest`
- Release for hardware team: `tools/make_release.sh <env> <tag>` → `dist/<tag>/eng401-<tag>.bin` (flash at 0x0), then `git tag <tag>`

## Unknowns / assumptions (keep updated)
| # | Item | Current assumption | How to resolve |
|---|------|--------------------|----------------|
| U1 | GNSS model | **Decided: u-blox NEO-M8N** (parts list is the baseline, list still awaiting instructor approval). Generic NMEA @ 9600 baud, 1 Hz; accept $GP/$GN/$GL talkers | Diag firmware prints raw NMEA |
| U2 | M8N connector is 6-pin 1.25 mm (drone type); pinout unknown, often includes a compass on I²C (SDA/SCL) | Use only VCC, GND, TX, RX; leave compass unconnected at first | Photo of module label / connector |
| U3 | GNSS module supply voltage (3.3 V or 5 V) | Do not assume; check module silkscreen | Photo from hardware team |
| U4 | PPS pin availability | No PPS → UTC offset accuracy ~±100 ms | Module photo |
| U5 | BME280 I²C address 0x76 vs 0x77 | Driver probes both | Diag I²C scan |
| U6 | VL53L0X vs VL53L1X (list says "decision pending") | VL53L0X (0x29) | Parts arrival |
| U7 | Power option A (powerbank) or B (18650) | A | Hardware team |
| U8 | Field Wi-Fi: laptop hotspot vs phone hotspot vs ESP32 soft-AP | ESP32 joins phone/laptop hotspot (STA); SSID/IP configurable | Decide before Week 9 |
| U9 | ESP32 USB chip CP2102 vs CH340 (driver for flashing) | Either; guide covers both | Hardware team |
| U10 | Wokwi support for BME280 / VL53L0X / GNSS | Not supported → mocks in sim | Verify when building wokwi/ |
| U11 | Current semester week | Plan relative to handout weeks | User |
