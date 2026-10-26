#include "nmea_basic.h"

#include <cstdlib>
#include <cstring>

namespace nmea {

bool LineBuffer::push(char c) {
  if (c == '$') {  // a '$' always starts a new sentence, even mid-line
    inSentence_ = true;
    overflowed_ = false;
    len_ = 0;
  }
  if (!inSentence_) return false;

  if (c == '\r' || c == '\n') {
    inSentence_ = false;
    if (overflowed_ || len_ == 0) return false;
    buf_[len_] = '\0';
    return true;
  }
  if (len_ >= kMaxSentenceLen) {
    if (!overflowed_) overflows_++;
    overflowed_ = true;
    return false;
  }
  buf_[len_++] = c;
  return false;
}

static int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool checksumOk(const char* s) {
  if (s == nullptr || s[0] != '$') return false;
  uint8_t sum = 0;
  const char* p = s + 1;
  while (*p != '\0' && *p != '*') sum ^= static_cast<uint8_t>(*p++);
  if (*p != '*') return false;
  int hi = hexValue(p[1]);
  int lo = (hi < 0) ? -1 : hexValue(p[2]);
  if (lo < 0 || p[3] != '\0') return false;
  return sum == static_cast<uint8_t>(hi * 16 + lo);
}

bool sentenceType(const char* s, char out[4]) {
  if (s == nullptr || s[0] != '$' || std::strlen(s) < 7 || s[6] != ',') return false;
  std::memcpy(out, s + 3, 3);
  out[3] = '\0';
  return true;
}

// Returns a pointer to the start of field `index` (0 = "$GPGGA"), or nullptr.
static const char* field(const char* s, int index) {
  const char* p = s;
  for (int i = 0; i < index; ++i) {
    p = std::strchr(p, ',');
    if (p == nullptr) return nullptr;
    ++p;
  }
  return p;
}

static bool fieldEmpty(const char* f) { return f == nullptr || *f == ',' || *f == '*'; }

bool parseGgaQuick(const char* s, GgaQuick& out) {
  char type[4];
  if (!checksumOk(s) || !sentenceType(s, type) || std::strcmp(type, "GGA") != 0) return false;
  // GGA fields: 6 = fix quality, 7 = satellites used, 8 = HDOP
  const char* fq = field(s, 6);
  const char* ns = field(s, 7);
  const char* hd = field(s, 8);
  out.fixQuality = fieldEmpty(fq) ? 0 : std::atoi(fq);
  out.satellites = fieldEmpty(ns) ? 0 : std::atoi(ns);
  out.hdop = fieldEmpty(hd) ? -1.0f : static_cast<float>(std::atof(hd));
  return true;
}

}  // namespace nmea
