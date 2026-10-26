# Wokwi simulation

Simulated: ESP32, MPU6050, microSD, 3 LEDs, button.
Not available in Wokwi (as far as checked): BME280, VL53L0X, GNSS → the diag firmware
reports them as FAIL, which exercises the error paths and report format.
The app firmware will use the `esp32-mock` env for those sensors.

Run:
1. `cd firmware && ../.venv/bin/pio run -e diag`
2. VS Code → install "Wokwi Simulator" extension (free licence) → open `wokwi/diagram.json`
   → "Wokwi: Start Simulator".

Status: diagram written from Wokwi docs, **not yet run** (needs the extension licence).
