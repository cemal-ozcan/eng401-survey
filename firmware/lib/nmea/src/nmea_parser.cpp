#include "nmea_parser.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "civil_time.h"

namespace nmea {

namespace {

constexpr int kMaxFields = 24;
constexpr float kKnotsToKmh = 1.852f;

bool isEmpty(const char* s) { return s == nullptr || *s == '\0'; }

bool allDigits(const char* s, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
  return true;
}

float toFloat(const char* s) { return isEmpty(s) ? NAN : static_cast<float>(std::atof(s)); }

// Splits "$GPGGA,a,,b*HH" in place into fields; field 0 is "$GPGGA".
// Empty fields are kept as "" so field indices match the NMEA spec.
int splitFields(char* s, char** fields) {
  char* star = std::strchr(s, '*');
  if (star) *star = '\0';
  int n = 0;
  fields[n++] = s;
  for (char* p = s; *p && n < kMaxFields; ++p) {
    if (*p == ',') {
      *p = '\0';
      fields[n++] = p + 1;
    }
  }
  return n;
}

}  // namespace

int64_t GnssData::utcMs() const {
  if (!timeValid || !dateValid) return -1;
  return epochDays * 86400000LL + timeOfDayMs;
}

double parseCoordinate(const char* value, const char* hemi) {
  if (isEmpty(value) || isEmpty(hemi)) return NAN;
  const char* dot = std::strchr(value, '.');
  size_t intLen = dot ? static_cast<size_t>(dot - value) : std::strlen(value);
  if (intLen < 3 || intLen > 5 || !allDigits(value, intLen)) return NAN;  // ddmm or dddmm
  double raw = std::atof(value);
  double degrees = std::floor(raw / 100.0);
  double minutes = raw - degrees * 100.0;
  if (minutes >= 60.0) return NAN;
  double deg = degrees + minutes / 60.0;
  switch (hemi[0]) {
    case 'N': case 'E': return deg;
    case 'S': case 'W': return -deg;
    default: return NAN;
  }
}

bool parseTimeOfDay(const char* v, uint32_t& msOfDay) {
  if (isEmpty(v) || std::strlen(v) < 6 || !allDigits(v, 6)) return false;
  unsigned h = (v[0] - '0') * 10 + (v[1] - '0');
  unsigned m = (v[2] - '0') * 10 + (v[3] - '0');
  unsigned s = (v[4] - '0') * 10 + (v[5] - '0');
  if (h > 23 || m > 59 || s > 60) return false;  // 60 = leap second
  unsigned frac = 0;
  if (v[6] == '.') {
    // up to 3 fractional digits -> ms
    unsigned scale = 100;
    for (const char* p = v + 7; *p && scale > 0; ++p, scale /= 10) {
      if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
      frac += (*p - '0') * scale;
    }
  }
  msOfDay = ((h * 60 + m) * 60 + s) * 1000 + frac;
  return true;
}

bool parseDate(const char* v, int64_t& epochDays) {
  if (isEmpty(v) || std::strlen(v) != 6 || !allDigits(v, 6)) return false;
  unsigned d = (v[0] - '0') * 10 + (v[1] - '0');
  unsigned m = (v[2] - '0') * 10 + (v[3] - '0');
  unsigned y = (v[4] - '0') * 10 + (v[5] - '0');
  if (d < 1 || d > 31 || m < 1 || m > 12) return false;
  int year = y >= 80 ? 1900 + static_cast<int>(y) : 2000 + static_cast<int>(y);
  epochDays = timeutil::daysFromCivil(year, m, d);
  return true;
}

Sentence Parser::feed(char c) {
  if (!line_.push(c)) return Sentence::None;
  return processSentence(line_.line());
}

Sentence Parser::processSentence(const char* sentence) {
  if (!checksumOk(sentence)) {
    ++stats_.checksumErrors;
    return Sentence::ChecksumError;
  }
  ++stats_.sentences;
  char type[4];
  if (!sentenceType(sentence, type)) return Sentence::Other;

  char buf[kMaxSentenceLen + 1];
  std::strncpy(buf, sentence, kMaxSentenceLen);
  buf[kMaxSentenceLen] = '\0';
  char* f[kMaxFields];
  int n = splitFields(buf, f);

  if (std::strcmp(type, "GGA") == 0 && parseGga(f, n)) { ++stats_.gga; return Sentence::GGA; }
  if (std::strcmp(type, "RMC") == 0 && parseRmc(f, n)) { ++stats_.rmc; return Sentence::RMC; }
  if (std::strcmp(type, "VTG") == 0 && parseVtg(f, n)) { ++stats_.vtg; return Sentence::VTG; }
  return Sentence::Other;
}

// $xxGGA,time,lat,N,lon,E,quality,numSV,HDOP,alt,M,sep,M,diffAge,diffStation
bool Parser::parseGga(char** f, int n) {
  if (n < 10) return false;
  uint32_t tod;
  if (parseTimeOfDay(f[1], tod)) {
    data_.timeOfDayMs = tod;
    data_.timeValid = true;
  }
  data_.fixQuality = isEmpty(f[6]) ? 0 : std::atoi(f[6]);
  data_.satellites = isEmpty(f[7]) ? 0 : std::atoi(f[7]);
  data_.hdop = toFloat(f[8]);

  double lat = parseCoordinate(f[2], f[3]);
  double lon = parseCoordinate(f[4], f[5]);
  if (data_.fixQuality > 0 && !std::isnan(lat) && !std::isnan(lon)) {
    data_.lat = lat;
    data_.lon = lon;
    data_.alt = toFloat(f[9]);
    data_.posValid = true;
  } else {
    data_.posValid = false;
  }
  return true;
}

// $xxRMC,time,status,lat,N,lon,E,spd(knots),cog,date,mv,mvE,mode
bool Parser::parseRmc(char** f, int n) {
  if (n < 10) return false;
  uint32_t tod;
  if (parseTimeOfDay(f[1], tod)) {
    data_.timeOfDayMs = tod;
    data_.timeValid = true;
  }
  int64_t days;
  if (parseDate(f[9], days)) {
    data_.epochDays = days;
    data_.dateValid = true;
  }
  bool active = f[2][0] == 'A';
  if (!active) {
    data_.posValid = false;
    data_.fixQuality = 0;
    return true;
  }
  double lat = parseCoordinate(f[3], f[4]);
  double lon = parseCoordinate(f[5], f[6]);
  if (!std::isnan(lat) && !std::isnan(lon)) {
    data_.lat = lat;
    data_.lon = lon;
    data_.posValid = true;
    if (data_.fixQuality == 0) data_.fixQuality = 1;  // RMC-only receivers
  }
  float knots = toFloat(f[7]);
  data_.speedKmh = std::isnan(knots) ? NAN : knots * kKnotsToKmh;
  data_.courseDeg = toFloat(f[8]);
  return true;
}

// $xxVTG,cogT,T,cogM,M,sogKnots,N,sogKmh,K,mode
bool Parser::parseVtg(char** f, int n) {
  if (n < 8) return false;
  float kmh = toFloat(f[7]);
  if (!std::isnan(kmh)) data_.speedKmh = kmh;
  float cog = toFloat(f[1]);
  if (!std::isnan(cog)) data_.courseDeg = cog;
  return true;
}

}  // namespace nmea
