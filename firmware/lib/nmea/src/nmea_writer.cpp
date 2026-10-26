#include "nmea_writer.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "civil_time.h"

namespace nmea {

namespace {

constexpr float kKmhToKnots = 1.0f / 1.852f;

// "ddmm.mmmmm,N" / "dddmm.mmmmm,E"
void formatCoord(double deg, bool isLat, char* out, size_t size) {
  char hemi = isLat ? (deg >= 0 ? 'N' : 'S') : (deg >= 0 ? 'E' : 'W');
  double a = std::fabs(deg);
  int d = static_cast<int>(a);
  double minutes = (a - d) * 60.0;
  if (minutes >= 59.999995) {  // avoid "60.00000" after rounding
    minutes = 0.0;
    ++d;
  }
  std::snprintf(out, size, isLat ? "%02d%08.5f,%c" : "%03d%08.5f,%c", d, minutes, hemi);
}

void formatTime(const Epoch& e, char* out, size_t size) {
  if (!e.timeValid) {
    out[0] = '\0';
    return;
  }
  uint32_t t = e.timeOfDayMs;
  std::snprintf(out, size, "%02u%02u%02u.%02u", static_cast<unsigned>(t / 3600000),
                static_cast<unsigned>((t / 60000) % 60), static_cast<unsigned>((t / 1000) % 60),
                static_cast<unsigned>((t % 1000) / 10));
}

void formatDate(const Epoch& e, char* out, size_t size) {
  if (!e.dateValid) {
    out[0] = '\0';
    return;
  }
  timeutil::CivilDate c = timeutil::civilFromDays(e.epochDays);
  std::snprintf(out, size, "%02u%02u%02d", c.day, c.month, c.year % 100);
}

}  // namespace

size_t finishSentence(char* s, size_t size) {
  size_t len = std::strlen(s);
  if (len < 2 || s[0] != '$' || len + 6 > size) return 0;  // "*HH\r\n" + NUL
  uint8_t sum = 0;
  for (size_t i = 1; i < len; ++i) sum ^= static_cast<uint8_t>(s[i]);
  std::snprintf(s + len, size - len, "*%02X\r\n", sum);
  return len + 5;
}

size_t writeEpoch(const Epoch& e, char* out, size_t size) {
  char tod[16], date[8], lat[20] = ",", lon[20] = ",";
  formatTime(e, tod, sizeof(tod));
  formatDate(e, date, sizeof(date));
  const bool fix = e.fixQuality > 0;
  if (fix) {
    formatCoord(e.lat, true, lat, sizeof(lat));
    formatCoord(e.lon, false, lon, sizeof(lon));
  }

  size_t used = 0;
  char line[100];
  auto emit = [&](void) -> bool {
    size_t n = finishSentence(line, sizeof(line));
    if (n == 0 || used + n >= size) return false;
    std::memcpy(out + used, line, n);
    used += n;
    out[used] = '\0';
    return true;
  };

  if (fix) {
    std::snprintf(line, sizeof(line), "$GNRMC,%s,A,%s,%s,%.3f,%.2f,%s,,,A", tod, lat, lon,
                  e.speedKmh * kKmhToKnots, e.courseDeg, date);
  } else {
    std::snprintf(line, sizeof(line), "$GNRMC,%s,V,,,,,,,%s,,,N", tod, date);
  }
  if (!emit()) return 0;

  if (fix) {
    std::snprintf(line, sizeof(line), "$GNVTG,%.2f,T,,M,%.3f,N,%.3f,K,A", e.courseDeg,
                  e.speedKmh * kKmhToKnots, e.speedKmh);
  } else {
    std::snprintf(line, sizeof(line), "$GNVTG,,,,,,,,,N");
  }
  if (!emit()) return 0;

  if (fix) {
    std::snprintf(line, sizeof(line), "$GNGGA,%s,%s,%s,%d,%02d,%.2f,%.1f,M,%.1f,M,,", tod, lat, lon,
                  e.fixQuality, e.satellites, e.hdop, e.alt, e.geoidSep);
  } else {
    std::snprintf(line, sizeof(line), "$GNGGA,%s,,,,,0,%02d,%.2f,,,,,,", tod, e.satellites, e.hdop);
  }
  if (!emit()) return 0;
  return used;
}

}  // namespace nmea
