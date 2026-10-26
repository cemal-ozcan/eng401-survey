#include "record_builder.h"

#include "utm.h"

namespace record {

void fillGnss(Record& r, const nmea::GnssData& g, uint64_t gnssUpdateUs, uint64_t nowUs) {
  r.fix = static_cast<uint8_t>(g.fixQuality < 0 ? 0 : g.fixQuality);
  r.sats = static_cast<uint8_t>(g.satellites < 0 ? 0 : g.satellites);
  r.hdop = g.hdop;
  r.gnssAgeMs = (gnssUpdateUs == 0 || nowUs < gnssUpdateUs)
                    ? kAgeUnknown
                    : static_cast<uint32_t>((nowUs - gnssUpdateUs) / 1000ULL);

  geo::Utm u{};
  if (g.posValid && geo::latLonToUtm(g.lat, g.lon, u)) {
    r.lat = g.lat;
    r.lon = g.lon;
    r.alt = g.alt;
    r.spd = g.speedKmh;
    r.cog = g.courseDeg;
    r.utmZone = u.zone;
    r.utmHemi = u.hemisphere;
    r.easting = u.easting;
    r.northing = u.northing;
    r.flags |= kFlagGnssFix;
  }
}

void fillImu(Record& r, const hal::ImuSample* s) {
  if (s == nullptr) return;
  r.ax = s->ax; r.ay = s->ay; r.az = s->az;
  r.gx = s->gx; r.gy = s->gy; r.gz = s->gz;
  r.flags |= kFlagImuOk;
}

void fillEnv(Record& r, const hal::EnvSample* s) {
  if (s == nullptr) return;
  r.temp = s->tempC;
  r.pres = s->pressureHpa;
  r.hum = s->humidityPct;
  r.flags |= kFlagEnvOk;
}

void fillRange(Record& r, const hal::RangeSample* s) {
  if (s == nullptr || !s->valid) return;
  r.dist = s->mm;
  r.flags |= kFlagDistOk;
}

}  // namespace record
