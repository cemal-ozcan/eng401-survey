#include "civil_time.h"

#include <cstdio>

namespace timeutil {

int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

CivilDate civilFromDays(int64_t z) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp < 10 ? mp + 3 : mp - 9;
  const int y = static_cast<int>(yoe + era * 400) + (m <= 2);
  return {y, m, d};
}

bool formatIso8601(int64_t ms, char* out, size_t outSize) {
  if (ms < 0 || outSize < kIsoLen) return false;
  const int64_t days = ms / 86400000;
  const int64_t msOfDay = ms % 86400000;
  const CivilDate c = civilFromDays(days);
  const unsigned h = static_cast<unsigned>(msOfDay / 3600000);
  const unsigned mi = static_cast<unsigned>((msOfDay / 60000) % 60);
  const unsigned s = static_cast<unsigned>((msOfDay / 1000) % 60);
  const unsigned frac = static_cast<unsigned>(msOfDay % 1000);
  int n = std::snprintf(out, outSize, "%04d-%02u-%02uT%02u:%02u:%02u.%03uZ", c.year, c.month, c.day, h,
                        mi, s, frac);
  return n == static_cast<int>(kIsoLen - 1);
}

}  // namespace timeutil
