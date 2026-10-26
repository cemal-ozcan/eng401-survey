#include "telemetry.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "record.h"

namespace telemetry {

size_t encodeHeartbeat(const Heartbeat& hb, char* out, size_t size) {
  int n = std::snprintf(out, size,
                        "{\"v\":%d,\"type\":\"hb\",\"t_us\":%llu,\"uptime_s\":%lu,\"heap\":%lu,\"rssi\":%ld,"
                        "\"sd_free_mb\":%lu,\"q_drop\":%lu,\"sd_err\":%lu,\"net_drop\":%lu,\"fw\":\"%s\"}",
                        record::kProtocolVersion, static_cast<unsigned long long>(hb.tUs),
                        static_cast<unsigned long>(hb.uptimeS), static_cast<unsigned long>(hb.heap),
                        static_cast<long>(hb.rssi), static_cast<unsigned long>(hb.sdFreeMb),
                        static_cast<unsigned long>(hb.queueDrops), static_cast<unsigned long>(hb.sdErrors),
                        static_cast<unsigned long>(hb.netDrops), hb.fw ? hb.fw : "");
  return (n < 0 || static_cast<size_t>(n) >= size) ? 0 : static_cast<size_t>(n);
}

bool jsonGetInt(const char* json, const char* key, int64_t& out) {
  char pattern[40];
  std::snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* p = std::strstr(json, pattern);
  if (p == nullptr) return false;
  p += std::strlen(pattern);
  while (*p == ' ') ++p;
  char* end = nullptr;
  long long v = std::strtoll(p, &end, 10);
  if (end == p) return false;
  out = v;
  return true;
}

bool parsePing(const char* json, Ping& out) {
  int64_t v = 0, id = 0, t1 = 0;
  if (std::strstr(json, "\"type\":\"ping\"") == nullptr) return false;
  if (!jsonGetInt(json, "v", v) || v != record::kProtocolVersion) return false;
  if (!jsonGetInt(json, "id", id) || !jsonGetInt(json, "t1", t1) || id < 0) return false;
  out.id = static_cast<uint32_t>(id);
  out.t1 = t1;
  return true;
}

size_t encodePong(const Ping& p, uint64_t t2, uint64_t t3, char* out, size_t size) {
  int n = std::snprintf(out, size, "{\"v\":%d,\"type\":\"pong\",\"id\":%lu,\"t1\":%lld,\"t2\":%llu,\"t3\":%llu}",
                        record::kProtocolVersion, static_cast<unsigned long>(p.id),
                        static_cast<long long>(p.t1), static_cast<unsigned long long>(t2),
                        static_cast<unsigned long long>(t3));
  return (n < 0 || static_cast<size_t>(n) >= size) ? 0 : static_cast<size_t>(n);
}

}  // namespace telemetry
