#include <Arduino.h>

#include "build_info.h"
#include "nmea_parser.h"
#include "record.h"
#include "utm.h"

namespace {

// Week-3 checkpoint demo (GNSS/UTM coordinate processing): decode recorded
// NMEA, convert to UTM and print the resulting survey record. Replaced by the
// FreeRTOS application in step 4.
const char* const kDemoNmea[] = {
    "$GNRMC,101542.00,A,3955.24620,N,03251.24660,E,1.944,87.20,021126,,,A*4B",
    "$GNGGA,101542.00,3955.24620,N,03251.24660,E,1,09,0.92,938.4,M,36.2,M,,*49",
    "$GNVTG,87.20,T,,M,1.944,N,3.600,K,A*13",
};

void runGnssUtmDemo() {
  nmea::Parser parser;
  for (const char* s : kDemoNmea) parser.processSentence(s);
  const nmea::GnssData& g = parser.data();

  record::Record r;
  r.tUs = esp_timer_get_time();
  r.utcMs = g.utcMs();
  r.fix = static_cast<uint8_t>(g.fixQuality);
  r.sats = static_cast<uint8_t>(g.satellites);
  r.hdop = g.hdop;
  r.lat = g.lat;
  r.lon = g.lon;
  r.alt = g.alt;
  r.spd = g.speedKmh;
  r.cog = g.courseDeg;

  uint32_t t0 = micros();
  geo::Utm u{};
  if (g.posValid && geo::latLonToUtm(g.lat, g.lon, u)) {
    r.utmZone = u.zone;
    r.utmHemi = u.hemisphere;
    r.easting = u.easting;
    r.northing = u.northing;
  }
  uint32_t utmUs = micros() - t0;

  char line[record::kMaxCsvLen];
  record::toCsv(r, line, sizeof(line));
  Serial.println("--- GNSS -> UTM demo ---");
  Serial.printf("lat %.8f lon %.8f -> zone %u%c E %.3f N %.3f (UTM took %lu us)\n", g.lat, g.lon, u.zone,
                u.hemisphere, u.easting, u.northing, static_cast<unsigned long>(utmUs));
  Serial.println(record::csvHeader());
  Serial.println(line);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\nENG401 survey firmware v%s (%s sensors)\n", build_info::version(),
                build_info::modeName());
  runGnssUtmDemo();
}

void loop() {
  // Placeholder: the application will run in FreeRTOS tasks; loop() stays idle.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
