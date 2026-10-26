# Interface Control Document (ICD) — v0.1 draft

Single source of truth for the SD CSV format and the ESP32 ↔ PC telemetry protocol.
Firmware (`firmware/lib/record`) and PC codec (`pc/surveypc/protocol.py`) must match this file.

## 1. Sampling model
- One **record** per sampling tick. Default period `SAMPLE_PERIOD_MS = 1000` (configurable, e.g. 200).
- Each record carries the latest value of every sensor plus its validity flag and age.
- `seq` increments by 1 every tick, including ticks where sensors failed → gaps in `seq` at
  the PC = lost packets; `missed` = ticks the firmware itself could not service on time.
- Completeness (handout §9) = rows with `fix>0` and all sensor flags OK / expected ticks × 100,
  expected ticks = (t_last − t_first) / period + 1.

## 2. CSV data dictionary (SD card, file `/SURVEY_NNN.CSV`, header row included)
| # | Column | Type | Unit | Description |
|---|--------|------|------|-------------|
| 1 | seq | uint32 | – | Record sequence number since boot, starts at 0 |
| 2 | t_us | uint64 | µs | `esp_timer` time when the tick was sampled (monotonic) |
| 3 | utc | string | ISO 8601 | `YYYY-MM-DDThh:mm:ss.sssZ`, empty until first GNSS time sync |
| 4 | fix | uint8 | – | 0 = no fix, 1 = GPS fix, 2 = DGPS (from GGA quality) |
| 5 | sats | uint8 | – | Satellites used |
| 6 | hdop | float | – | Horizontal dilution of precision |
| 7 | lat | double | deg | WGS84 latitude, + = North (8 decimals) |
| 8 | lon | double | deg | WGS84 longitude, + = East (8 decimals) |
| 9 | alt | float | m | Altitude above MSL (GGA) |
| 10 | spd | float | km/h | Speed over ground (VTG/RMC) |
| 11 | cog | float | deg | Course over ground |
| 12 | gnss_age_ms | uint32 | ms | Age of the GNSS solution at sample time |
| 13 | utm_zone | uint8 | – | UTM zone 1–60 |
| 14 | utm_hemi | char | – | `N` / `S` |
| 15 | easting | double | m | UTM Easting (3 decimals) |
| 16 | northing | double | m | UTM Northing (3 decimals) |
| 17–19 | ax, ay, az | float | m/s² | Accelerometer |
| 20–22 | gx, gy, gz | float | deg/s | Gyroscope |
| 23 | temp | float | °C | BME280 temperature |
| 24 | pres | float | hPa | BME280 pressure |
| 25 | hum | float | %RH | BME280 humidity |
| 26 | dist | uint16 | mm | VL53L0X range (0 if invalid) |
| 27 | flags | uint16 | bitmask | Sensor validity, see §2.1 |
| 28 | missed | uint32 | – | Cumulative missed ticks |
| 29 | mark | uint16 | – | Observation-point id (button press), 0 = none |

Invalid numeric values are written as empty fields (not 0), except `dist` (see flags).

### 2.1 `flags` bits
| Bit | Meaning when 1 |
|-----|----------------|
| 0 | GNSS fix valid |
| 1 | IMU read OK |
| 2 | BME280 read OK |
| 3 | VL53L0X range valid |
| 4 | SD write OK (previous flush) |
| 5 | Wi-Fi connected |
| 6 | UTC synced |
| 7 | Running with MOCK sensors |

### 2.2 Example row
```
seq,t_us,utc,fix,sats,hdop,lat,lon,alt,spd,cog,gnss_age_ms,utm_zone,utm_hemi,easting,northing,ax,ay,az,gx,gy,gz,temp,pres,hum,dist,flags,missed,mark
42,43012345,2026-11-02T10:15:42.000Z,1,9,0.9,39.89123456,32.78012345,938.4,3.6,87.2,180,36,N,481234.567,4415678.901,0.12,-0.05,9.79,0.3,-0.1,0.0,18.4,912.3,41.0,1532,127,0,0
```
(Coordinates illustrative only.)

## 3. Telemetry protocol (Wi-Fi, UDP)
Why UDP: no head-of-line blocking or hidden retransmissions, so loss and latency are measured
honestly; `seq` exposes every lost datagram. Full dataset is always on SD anyway.

- ESP32 → PC: `PC_IP:5005` (PC IP configurable; broadcast allowed). PC → ESP32: `ESP_IP:5006`.
- Payload: one UTF-8 JSON object per datagram, `"v": 1` protocol version, `"type"` field.

### 3.1 `data` (every tick, or every N-th tick if `TELEM_DECIMATE > 1`)
Keys are the CSV column names (§2), invalid values are `null`:
```json
{"v":1,"type":"data","seq":42,"t_us":43012345,"utc":"2026-11-02T10:15:42.000Z","fix":1,"sats":9,
 "hdop":0.9,"lat":39.89123456,"lon":32.78012345,"alt":938.4,"spd":3.6,"cog":87.2,"gnss_age_ms":180,
 "utm_zone":36,"utm_hemi":"N","easting":481234.567,"northing":4415678.901,
 "ax":0.12,"ay":-0.05,"az":9.79,"gx":0.3,"gy":-0.1,"gz":0.0,"temp":18.4,"pres":912.3,"hum":41.0,
 "dist":1532,"flags":127,"missed":0,"mark":0}
```

### 3.2 `hb` heartbeat (1 Hz, independent of data)
```json
{"v":1,"type":"hb","t_us":43100000,"uptime_s":43,"heap":182344,"rssi":-61,"sd_free_mb":7420,"q_drop":0,"fw":"0.1.0"}
```

### 3.3 Latency measurement: `ping` / `pong` (NTP-style)
1. PC sends `{"v":1,"type":"ping","id":7,"t1":<pc_time_us>}`.
2. ESP32 replies `{"v":1,"type":"pong","id":7,"t1":…,"t2":<esp rx t_us>,"t3":<esp tx t_us>}`.
3. PC records `t4` on receive. Then
   `rtt = (t4 − t1) − (t3 − t2)`, `offset = ((t2 − t1) + (t3 − t4)) / 2`  (ESP clock − PC clock).
4. Offset estimate = value from the ping with the **smallest rtt** in a sliding window (≈1 Hz pings).
5. Sensor-to-PC latency for each data packet: `lat = pc_rx_us − (t_us − offset)`.
   Reported as min / mean / max (handout §9). Uncertainty ≈ rtt_min / 2 — reported alongside.

## 4. PC receiver log (`data/processed/telemetry_*.csv`)
All §2 columns + `pc_rx_us` (PC monotonic receive time) + `latency_ms`.
