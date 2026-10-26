#pragma once

// Calendar <-> Unix time conversion without libc timezone machinery
// (H. Hinnant's days_from_civil / civil_from_days algorithms).

#include <cstddef>
#include <cstdint>

namespace timeutil {

// Days since 1970-01-01 for a proleptic Gregorian date.
int64_t daysFromCivil(int year, unsigned month, unsigned day);

struct CivilDate {
  int year;
  unsigned month;
  unsigned day;
};
CivilDate civilFromDays(int64_t days);

// Unix epoch milliseconds -> "YYYY-MM-DDThh:mm:ss.sssZ" (24 chars + NUL).
// Returns false if out is too small or ms is negative.
constexpr size_t kIsoLen = 25;
bool formatIso8601(int64_t epochMs, char* out, size_t outSize);

}  // namespace timeutil
