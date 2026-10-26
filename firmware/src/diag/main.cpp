// ENG401 diagnostic firmware.
//
// Runs once at boot and then every kRerunPeriodMs: scans the I2C bus, tests every
// sensor, listens to the GNSS UART, tests the SD card and blinks the LEDs. Prints a
// plain-text report that non-experts copy back to the software owner.
//
// This is a sequential test tool, not the survey application: blocking waits are
// acceptable here (the no-delay rule applies to the app tasks in src/app/).

#include <Adafruit_BME280.h>
#include <Adafruit_MPU6050.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <VL53L0X.h>
#include <Wire.h>

#include <algorithm>

#include "build_info.h"
#include "chip_id.h"
#include "nmea_basic.h"
#include "pins.h"
#include "report.h"

namespace {

constexpr uint32_t kRerunPeriodMs = 30000;
constexpr uint32_t kBaudProbeMs = 2500;
constexpr uint32_t kGnssListenMs = 8000;
constexpr size_t kRawNmeaLines = 6;

diag::Report report;
uint32_t runNumber = 0;
bool buttonSeen = false;

// ---------------------------------------------------------------- I2C helpers

uint8_t foundAddrs[16];
size_t foundCount = 0;

bool i2cPresent(uint8_t addr) {
  for (size_t i = 0; i < foundCount; ++i)
    if (foundAddrs[i] == addr) return true;
  return false;
}

bool readReg8(uint8_t addr, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, static_cast<uint8_t>(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool readReg16Addr16(uint8_t addr, uint16_t reg, uint16_t& value) {
  Wire.beginTransmission(addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, static_cast<uint8_t>(2)) != 2) return false;
  value = static_cast<uint16_t>(Wire.read() << 8);
  value |= Wire.read();
  return true;
}

// ---------------------------------------------------------------- checks

void checkI2c() {
  Serial.println("\n[1] I2C bus scan (SDA=GPIO21, SCL=GPIO22)");
  Wire.end();

  // Both lines idle HIGH (pull-ups on the sensor boards). LOW means a short or
  // a module pulling the bus down.
  pinMode(pins::kI2cSda, INPUT_PULLUP);
  pinMode(pins::kI2cScl, INPUT_PULLUP);
  delay(5);
  bool sdaLow = digitalRead(pins::kI2cSda) == LOW;
  bool sclLow = digitalRead(pins::kI2cScl) == LOW;

  Wire.begin(pins::kI2cSda, pins::kI2cScl, 100000);
  Wire.setTimeOut(50);

  foundCount = 0;
  char list[64] = "";
  for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0 && foundCount < sizeof(foundAddrs)) {
      foundAddrs[foundCount++] = addr;
      Serial.printf("    found 0x%02X  -> %s\n", addr, diag::nameForAddress(addr));
      char item[6];
      snprintf(item, sizeof(item), "0x%02X ", addr);
      strncat(list, item, sizeof(list) - strlen(list) - 1);
    }
  }

  char detail[112];
  if (sdaLow || sclLow) {
    snprintf(detail, sizeof(detail), "%s%s stuck LOW: short to GND or bad wire", sdaLow ? "SDA " : "",
             sclLow ? "SCL " : "");
    report.set("I2C", diag::Status::Fail, detail);
  } else if (foundCount == 0) {
    report.set("I2C", diag::Status::Fail,
               "no devices: check SDA->GPIO21, SCL->GPIO22, sensor VCC->3V3, GND");
  } else {
    snprintf(detail, sizeof(detail), "%u device(s): %s", static_cast<unsigned>(foundCount), list);
    report.set("I2C", diag::Status::Ok, detail);
  }
}

void checkMpu() {
  Serial.println("\n[2] MPU6050 (IMU)");
  uint8_t addr = i2cPresent(0x68) ? 0x68 : (i2cPresent(0x69) ? 0x69 : 0);
  if (addr == 0) {
    report.set("MPU6050", diag::Status::Fail, "not found at 0x68/0x69: check wires, AD0->GND");
    return;
  }
  uint8_t who = 0;
  char detail[112];
  if (!readReg8(addr, diag::kMpuWhoAmIReg, who)) {
    report.set("MPU6050", diag::Status::Fail, "answers scan but WHO_AM_I read failed");
    return;
  }
  Serial.printf("    WHO_AM_I = 0x%02X (%s)\n", who, diag::mpuName(who));
  if (!diag::isMpu6050(who)) {
    snprintf(detail, sizeof(detail), "chip is %s (WHO_AM_I 0x%02X), not MPU6050: tell software",
             diag::mpuName(who), who);
    report.set("MPU6050", diag::Status::Warn, detail);
    return;
  }

  Adafruit_MPU6050 mpu;
  if (!mpu.begin(addr, &Wire)) {
    report.set("MPU6050", diag::Status::Fail, "library init failed");
    return;
  }
  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0, t = 0;
  constexpr int kSamples = 20;
  for (int i = 0; i < kSamples; ++i) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    ax += a.acceleration.x; ay += a.acceleration.y; az += a.acceleration.z;
    gx += g.gyro.x; gy += g.gyro.y; gz += g.gyro.z;
    t += temp.temperature;
    delay(10);
  }
  ax /= kSamples; ay /= kSamples; az /= kSamples;
  constexpr float kRadToDeg = 57.29578f;
  gx *= kRadToDeg / kSamples; gy *= kRadToDeg / kSamples; gz *= kRadToDeg / kSamples;
  t /= kSamples;
  float mag = sqrtf(ax * ax + ay * ay + az * az);

  snprintf(detail, sizeof(detail), "0x%02X a=(%.2f,%.2f,%.2f) |a|=%.2f m/s2 g=(%.1f,%.1f,%.1f) dps",
           addr, ax, ay, az, mag, gx, gy, gz);
  // Stationary board: acceleration magnitude must be close to gravity.
  bool plausible = mag > 8.0f && mag < 11.5f;
  report.set("MPU6050", plausible ? diag::Status::Ok : diag::Status::Warn, detail);
  Serial.printf("    %s, chip temp %.1f C%s\n", detail, t,
                plausible ? "" : "  <- |a| should be ~9.8 when the board is still");
}

void checkBme() {
  Serial.println("\n[3] BME280 (temperature / pressure / humidity)");
  uint8_t addr = i2cPresent(0x76) ? 0x76 : (i2cPresent(0x77) ? 0x77 : 0);
  if (addr == 0) {
    report.set("BME280", diag::Status::Fail, "not found at 0x76/0x77: check wires, VCC->3V3");
    return;
  }
  uint8_t id = 0;
  char detail[112];
  if (!readReg8(addr, diag::kBoschChipIdReg, id)) {
    report.set("BME280", diag::Status::Fail, "answers scan but chip-id read failed");
    return;
  }
  Serial.printf("    chip id = 0x%02X (%s)\n", id, diag::boschName(id));
  if (!diag::isBme280(id)) {
    snprintf(detail, sizeof(detail), "chip is %s (id 0x%02X), not BME280", diag::boschName(id), id);
    report.set("BME280", diag::Status::Fail, detail);
    return;
  }

  Adafruit_BME280 bme;
  if (!bme.begin(addr, &Wire)) {
    report.set("BME280", diag::Status::Fail, "library init failed");
    return;
  }
  float t = bme.readTemperature();
  float p = bme.readPressure() / 100.0f;
  float h = bme.readHumidity();
  bool plausible = t > -20 && t < 60 && p > 300 && p < 1100 && h >= 0 && h <= 100;
  snprintf(detail, sizeof(detail), "0x%02X, %.1f C, %.1f hPa, %.1f %%RH", addr, t, p, h);
  report.set("BME280", plausible ? diag::Status::Ok : diag::Status::Warn, detail);
  Serial.printf("    %s%s\n", detail, plausible ? "" : "  <- values out of normal range");
}

void checkTof() {
  Serial.println("\n[4] VL53L0X (distance)");
  if (!i2cPresent(0x29)) {
    report.set("VL53L0X", diag::Status::Fail, "not found at 0x29: check wires, XSHUT->GPIO33");
    return;
  }
  uint8_t l0x = 0;
  uint16_t l1x = 0;
  readReg8(0x29, diag::kVl53l0xIdReg, l0x);
  if (l0x != 0xEE) readReg16Addr16(0x29, diag::kVl53l1xIdReg, l1x);
  const char* name = diag::tofName(l0x, l1x);
  Serial.printf("    model id: 0x%02X / 0x%04X (%s)\n", l0x, l1x, name);

  char detail[112];
  if (l0x != 0xEE) {
    snprintf(detail, sizeof(detail), "chip is %s, not VL53L0X: tell software", name);
    report.set("VL53L0X", diag::Status::Warn, detail);
    return;
  }

  VL53L0X tof;
  tof.setBus(&Wire);
  tof.setTimeout(500);
  if (!tof.init()) {
    report.set("VL53L0X", diag::Status::Fail, "library init failed");
    return;
  }
  uint16_t r[5];
  int timeouts = 0;
  for (auto& v : r) {
    v = tof.readRangeSingleMillimeters();
    if (tof.timeoutOccurred()) ++timeouts;
  }
  std::sort(r, r + 5);
  uint16_t median = r[2];
  Serial.printf("    readings (mm): %u %u %u %u %u, timeouts=%d\n", r[0], r[1], r[2], r[3], r[4],
                timeouts);
  if (timeouts > 0) {
    report.set("VL53L0X", diag::Status::Fail, "measurement timeouts");
  } else if (median >= 8000) {
    report.set("VL53L0X", diag::Status::Warn,
               "no target in range: point it at a wall 30-100 cm away and re-run");
  } else {
    snprintf(detail, sizeof(detail), "%u mm (median of 5)", median);
    report.set("VL53L0X", diag::Status::Ok, detail);
  }
}

struct GnssStats {
  uint32_t bytes = 0;
  uint32_t valid = 0;
  uint32_t invalid = 0;
};

GnssStats listenGnss(uint32_t baud, uint32_t durationMs, bool keepSample, char raw[][nmea::kMaxSentenceLen + 1],
                     size_t& rawCount, nmea::GgaQuick* lastGga, char talker[3]) {
  Serial2.end();
  Serial2.begin(baud, SERIAL_8N1, pins::kGnssRx, pins::kGnssTx);
  while (Serial2.available()) Serial2.read();

  GnssStats st;
  nmea::LineBuffer lb;
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    while (Serial2.available()) {
      ++st.bytes;
      if (!lb.push(static_cast<char>(Serial2.read()))) continue;
      const char* line = lb.line();
      if (!nmea::checksumOk(line)) {
        ++st.invalid;
        continue;
      }
      ++st.valid;
      if (talker) { talker[0] = line[1]; talker[1] = line[2]; talker[2] = '\0'; }
      nmea::GgaQuick g;
      if (lastGga && nmea::parseGgaQuick(line, g)) *lastGga = g;
      if (keepSample && rawCount < kRawNmeaLines) {
        strncpy(raw[rawCount], line, nmea::kMaxSentenceLen);
        raw[rawCount][nmea::kMaxSentenceLen] = '\0';
        ++rawCount;
      }
    }
    delay(2);
  }
  return st;
}

void checkGnss() {
  Serial.println("\n[5] GNSS (GNSS TX -> GPIO16, GNSS RX -> GPIO17)");
  static const uint32_t kBauds[] = {9600, 38400, 115200, 57600, 4800};
  static char raw[kRawNmeaLines][nmea::kMaxSentenceLen + 1];
  size_t rawCount = 0;
  uint32_t chosenBaud = 0, totalBytes = 0;

  for (uint32_t baud : kBauds) {
    GnssStats st = listenGnss(baud, kBaudProbeMs, false, raw, rawCount, nullptr, nullptr);
    Serial.printf("    probe %6lu baud: %lu bytes, %lu valid sentences\n", static_cast<unsigned long>(baud),
                  static_cast<unsigned long>(st.bytes), static_cast<unsigned long>(st.valid));
    totalBytes += st.bytes;
    if (st.valid >= 2) {
      chosenBaud = baud;
      break;
    }
  }

  char detail[112];
  if (chosenBaud == 0) {
    report.set("GNSS", diag::Status::Fail,
               totalBytes == 0 ? "no data: check GNSS power, GND, GNSS TX->GPIO16; try swapping TX/RX"
                               : "bytes received but unreadable at all baud rates: check TX/RX/GND");
    return;
  }

  Serial.printf("    listening %lu s at %lu baud...\n", static_cast<unsigned long>(kGnssListenMs / 1000),
                static_cast<unsigned long>(chosenBaud));
  nmea::GgaQuick gga;
  bool haveGga = false;
  gga.fixQuality = -1;
  char talker[3] = "??";
  GnssStats st = listenGnss(chosenBaud, kGnssListenMs, true, raw, rawCount, &gga, talker);
  haveGga = gga.fixQuality >= 0;

  Serial.println("    --- raw NMEA sample ---");
  for (size_t i = 0; i < rawCount; ++i) Serial.printf("    %s\n", raw[i]);
  Serial.printf("    valid=%lu invalid=%lu\n", static_cast<unsigned long>(st.valid),
                static_cast<unsigned long>(st.invalid));

  if (!haveGga) {
    snprintf(detail, sizeof(detail), "%lu baud, talker %s, data OK but no GGA sentence seen",
             static_cast<unsigned long>(chosenBaud), talker);
    report.set("GNSS", diag::Status::Warn, detail);
  } else if (gga.fixQuality == 0) {
    snprintf(detail, sizeof(detail),
             "%lu baud, talker %s, data OK, NO FIX yet (sats=%d): go outdoors, wait 1-5 min",
             static_cast<unsigned long>(chosenBaud), talker, gga.satellites);
    report.set("GNSS", diag::Status::Warn, detail);
  } else {
    snprintf(detail, sizeof(detail), "%lu baud, talker %s, FIX OK, sats=%d, HDOP=%.1f",
             static_cast<unsigned long>(chosenBaud), talker, gga.satellites, gga.hdop);
    report.set("GNSS", diag::Status::Ok, detail);
  }
}

const char* cardTypeName(uint8_t t) {
  switch (t) {
    case CARD_MMC: return "MMC";
    case CARD_SD: return "SDSC";
    case CARD_SDHC: return "SDHC";
    default: return "unknown";
  }
}

void checkSd() {
  Serial.println("\n[6] microSD (CS=5, SCK=18, MOSI=23, MISO=19)");
  SD.end();
  SPI.end();
  SPI.begin(pins::kSdSck, pins::kSdMiso, pins::kSdMosi, pins::kSdCs);
  if (!SD.begin(pins::kSdCs, SPI, 4000000)) {
    report.set("SD", diag::Status::Fail,
               "card not detected: card inserted? FAT32? VCC->5V, CS/SCK/MOSI/MISO wires");
    return;
  }
  uint8_t type = SD.cardType();
  if (type == CARD_NONE) {
    report.set("SD", diag::Status::Fail, "module OK but no card inserted");
    return;
  }
  uint32_t sizeMb = static_cast<uint32_t>(SD.cardSize() / (1024ULL * 1024ULL));
  uint32_t freeMb = static_cast<uint32_t>((SD.totalBytes() - SD.usedBytes()) / (1024ULL * 1024ULL));

  // Write a 4 KB pattern, read it back, compare, delete.
  static const char* kPath = "/diag_test.txt";
  static uint8_t buf[4096];
  for (size_t i = 0; i < sizeof(buf); ++i) buf[i] = static_cast<uint8_t>('A' + (i % 26));

  char detail[112];
  uint32_t t0 = millis();
  File f = SD.open(kPath, FILE_WRITE);
  size_t written = f ? f.write(buf, sizeof(buf)) : 0;
  if (f) f.close();
  uint32_t writeMs = millis() - t0;

  bool same = false;
  f = SD.open(kPath, FILE_READ);
  if (f) {
    same = f.size() == sizeof(buf);
    for (size_t i = 0; same && i < sizeof(buf); ++i) same = f.read() == buf[i];
    f.close();
  }
  SD.remove(kPath);

  if (written != sizeof(buf)) {
    snprintf(detail, sizeof(detail), "%s %lu MB, WRITE FAILED (card full or write-protected?)",
             cardTypeName(type), static_cast<unsigned long>(sizeMb));
    report.set("SD", diag::Status::Fail, detail);
  } else if (!same) {
    report.set("SD", diag::Status::Fail, "write OK but read-back mismatch: loose wires / bad card");
  } else {
    snprintf(detail, sizeof(detail), "%s %lu MB, free %lu MB, 4 KB write %lu ms, read-back OK",
             cardTypeName(type), static_cast<unsigned long>(sizeMb), static_cast<unsigned long>(freeMb),
             static_cast<unsigned long>(writeMs));
    report.set("SD", diag::Status::Ok, detail);
  }
}

void blink(uint8_t pin, const char* label) {
  Serial.printf("    %s should blink 3 times now\n", label);
  for (int i = 0; i < 3; ++i) {
    digitalWrite(pin, HIGH);
    delay(250);
    digitalWrite(pin, LOW);
    delay(250);
  }
}

void checkLeds() {
  Serial.println("\n[7] LEDs (watch the board)");
  blink(pins::kLedFix, "GREEN (GPIO25)");
  blink(pins::kLedLog, "BLUE  (GPIO26)");
  blink(pins::kLedError, "RED   (GPIO27)");
  report.set("LEDS", diag::Status::Skipped, "check by eye: write which colours blinked");
}

void updateButtonResult() {
  if (buttonSeen)
    report.set("BUTTON", diag::Status::Ok, "press detected on GPIO32");
  else
    report.set("BUTTON", diag::Status::Warn, "no press yet: press the button once");
}

void printReport() {
  Serial.println("\n==================== COPY FROM HERE ====================");
  Serial.printf("ENG401 DIAG v%s  run #%lu  uptime %lu s\n", build_info::version(),
                static_cast<unsigned long>(runNumber), static_cast<unsigned long>(millis() / 1000));
  uint64_t mac = ESP.getEfuseMac();
  Serial.printf("chip %s rev %d, MAC %012llX, flash %lu MB, free heap %lu B\n", ESP.getChipModel(),
                ESP.getChipRevision(), mac, static_cast<unsigned long>(ESP.getFlashChipSize() >> 20),
                static_cast<unsigned long>(ESP.getFreeHeap()));
  for (size_t i = 0; i < report.count(); ++i) {
    const auto& r = report.at(i);
    Serial.printf("[%-7s] %-8s: %s\n", diag::statusText(r.status), r.name, r.detail);
  }
  char line[80];
  report.summaryLine(line, sizeof(line));
  Serial.println(line);
  Serial.println("==================== COPY UNTIL HERE ===================");
  Serial.printf("Next run in %lu s. Press 'r' + Enter in the serial monitor to run now.\n",
                static_cast<unsigned long>(kRerunPeriodMs / 1000));
}

void runAll() {
  ++runNumber;
  digitalWrite(pins::kLedError, LOW);
  digitalWrite(pins::kLedFix, LOW);
  Serial.printf("\n\n######## ENG401 DIAGNOSTIC RUN #%lu ########\n", static_cast<unsigned long>(runNumber));
  checkI2c();
  checkMpu();
  checkBme();
  checkTof();
  checkGnss();
  checkSd();
  checkLeds();
  updateButtonResult();
  printReport();
  // Steady LED afterwards: red = at least one FAIL, green = no FAIL.
  bool anyFail = report.countWith(diag::Status::Fail) > 0;
  digitalWrite(anyFail ? pins::kLedError : pins::kLedFix, HIGH);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);  // give the serial monitor time to connect

  pinMode(pins::kLedFix, OUTPUT);
  pinMode(pins::kLedLog, OUTPUT);
  pinMode(pins::kLedError, OUTPUT);
  pinMode(pins::kButton, INPUT_PULLUP);
  pinMode(pins::kTofXshut, OUTPUT);
  digitalWrite(pins::kTofXshut, HIGH);  // keep VL53L0X out of shutdown
  delay(10);

  Serial.printf("\nENG401 diagnostic firmware v%s\n", build_info::version());
  runAll();
}

void loop() {
  static uint32_t lastRun = millis();
  static bool lastPressed = false;

  bool pressed = digitalRead(pins::kButton) == LOW;
  if (pressed && !lastPressed) {
    buttonSeen = true;
    Serial.println(">>> BUTTON pressed (GPIO32 OK)");
    digitalWrite(pins::kLedLog, HIGH);
  } else if (!pressed && lastPressed) {
    digitalWrite(pins::kLedLog, LOW);
  }
  lastPressed = pressed;

  bool rerunRequested = false;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == 'r' || c == 'R') rerunRequested = true;
  }

  if (rerunRequested || millis() - lastRun >= kRerunPeriodMs) {
    runAll();
    lastRun = millis();
  }
  delay(20);  // button debounce / idle
}
