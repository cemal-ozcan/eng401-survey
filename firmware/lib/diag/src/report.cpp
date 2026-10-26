#include "report.h"

#include <cstdio>
#include <cstring>

namespace diag {

const char* statusText(Status s) {
  switch (s) {
    case Status::Ok: return "OK";
    case Status::Warn: return "WARN";
    case Status::Fail: return "FAIL";
    case Status::Skipped: return "SKIPPED";
  }
  return "?";
}

static void copyTruncated(char* dst, size_t dstSize, const char* src) {
  std::snprintf(dst, dstSize, "%s", src ? src : "");
}

void Report::set(const char* name, Status status, const char* detail) {
  CheckResult* slot = nullptr;
  for (size_t i = 0; i < count_; ++i) {
    if (std::strncmp(results_[i].name, name, sizeof(results_[i].name) - 1) == 0) {
      slot = &results_[i];
      break;
    }
  }
  if (slot == nullptr) {
    if (count_ >= kMaxChecks) return;
    slot = &results_[count_++];
  }
  copyTruncated(slot->name, sizeof(slot->name), name);
  slot->status = status;
  copyTruncated(slot->detail, sizeof(slot->detail), detail);
}

size_t Report::countWith(Status s) const {
  size_t n = 0;
  for (size_t i = 0; i < count_; ++i) n += (results_[i].status == s);
  return n;
}

size_t Report::summaryLine(char* out, size_t outSize) const {
  int n = std::snprintf(out, outSize, "=== REPORT: %u/%u OK, %u WARN, %u FAIL ===",
                        static_cast<unsigned>(countWith(Status::Ok)),
                        static_cast<unsigned>(count_),
                        static_cast<unsigned>(countWith(Status::Warn)),
                        static_cast<unsigned>(countWith(Status::Fail)));
  return n < 0 ? 0 : static_cast<size_t>(n);
}

}  // namespace diag
