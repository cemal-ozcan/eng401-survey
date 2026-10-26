#include "record.h"

#include <cstdarg>
#include <cstdio>

#include "civil_time.h"

namespace record {

namespace {

// Appends formatted text to a fixed buffer; remembers overflow.
class Writer {
 public:
  Writer(char* out, size_t size) : out_(out), size_(size) {
    if (size_ > 0) out_[0] = '\0';
  }

  void put(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    if (!ok_) return;
    va_list args;
    va_start(args, fmt);
    int n = std::vsnprintf(out_ + len_, size_ - len_, fmt, args);
    va_end(args);
    if (n < 0 || static_cast<size_t>(n) >= size_ - len_) {
      ok_ = false;
      return;
    }
    len_ += static_cast<size_t>(n);
  }

  size_t result() const { return ok_ ? len_ : 0; }

 private:
  char* out_;
  size_t size_;
  size_t len_ = 0;
  bool ok_ = true;
};

// The two encodings differ only in separators and how a missing value looks.
class Encoder {
 public:
  Encoder(Writer& w, bool json) : w_(w), json_(json) {}

  void u(const char* key, unsigned long long v, bool valid = true) {
    begin(key);
    if (valid) w_.put("%llu", v);
    else missing();
  }
  void f(const char* key, double v, int decimals) {
    begin(key);
    if (std::isnan(v)) missing();
    else w_.put("%.*f", decimals, v);
  }
  void s(const char* key, const char* v, bool valid) {
    begin(key);
    if (!valid) missing();
    else if (json_) w_.put("\"%s\"", v);
    else w_.put("%s", v);
  }

 private:
  void begin(const char* key) {
    if (!first_) w_.put(",");
    first_ = false;
    if (json_) w_.put("\"%s\":", key);
  }
  void missing() {
    if (json_) w_.put("null");
  }

  Writer& w_;
  bool json_;
  bool first_ = true;
};

void encodeFields(const Record& r, Encoder& e) {
  char iso[timeutil::kIsoLen];
  bool utcOk = timeutil::formatIso8601(r.utcMs, iso, sizeof(iso));
  bool utmOk = r.utmZone != 0 && !std::isnan(r.easting) && !std::isnan(r.northing);
  char hemi[2] = {r.utmHemi, '\0'};

  e.u("seq", r.seq);
  e.u("t_us", r.tUs);
  e.s("utc", iso, utcOk);
  e.u("fix", r.fix);
  e.u("sats", r.sats);
  e.f("hdop", r.hdop, 2);
  e.f("lat", r.lat, 8);
  e.f("lon", r.lon, 8);
  e.f("alt", r.alt, 1);
  e.f("spd", r.spd, 2);
  e.f("cog", r.cog, 1);
  e.u("gnss_age_ms", r.gnssAgeMs, r.gnssAgeMs != kAgeUnknown);
  e.u("utm_zone", r.utmZone, utmOk);
  e.s("utm_hemi", hemi, utmOk);
  e.f("easting", utmOk ? r.easting : NAN, 3);
  e.f("northing", utmOk ? r.northing : NAN, 3);
  e.f("ax", r.ax, 3);
  e.f("ay", r.ay, 3);
  e.f("az", r.az, 3);
  e.f("gx", r.gx, 2);
  e.f("gy", r.gy, 2);
  e.f("gz", r.gz, 2);
  e.f("temp", r.temp, 2);
  e.f("pres", r.pres, 2);
  e.f("hum", r.hum, 1);
  e.u("dist", r.dist);
  e.u("flags", r.flags);
  e.u("missed", r.missed);
  e.u("mark", r.mark);
}

}  // namespace

const char* csvHeader() {
  return "seq,t_us,utc,fix,sats,hdop,lat,lon,alt,spd,cog,gnss_age_ms,utm_zone,utm_hemi,easting,"
         "northing,ax,ay,az,gx,gy,gz,temp,pres,hum,dist,flags,missed,mark";
}

size_t toCsv(const Record& r, char* out, size_t outSize) {
  Writer w(out, outSize);
  Encoder e(w, false);
  encodeFields(r, e);
  return w.result();
}

size_t toJson(const Record& r, char* out, size_t outSize) {
  Writer w(out, outSize);
  w.put("{\"v\":%d,\"type\":\"data\",", kProtocolVersion);
  Encoder e(w, true);
  encodeFields(r, e);
  w.put("}");
  return w.result();
}

}  // namespace record
