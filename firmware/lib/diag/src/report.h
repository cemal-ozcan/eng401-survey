#pragma once

// Collects the outcome of each diagnostic check and renders the summary
// block that the hardware team copies back to the software owner.

#include <cstddef>
#include <cstdint>

namespace diag {

enum class Status : uint8_t { Ok, Warn, Fail, Skipped };

const char* statusText(Status s);

struct CheckResult {
  char name[16];
  Status status;
  char detail[112];
};

class Report {
 public:
  static constexpr size_t kMaxChecks = 12;

  void clear() { count_ = 0; }
  // Adds or replaces the result for `name`. Detail is truncated if too long.
  void set(const char* name, Status status, const char* detail);

  size_t count() const { return count_; }
  const CheckResult& at(size_t i) const { return results_[i]; }
  size_t countWith(Status s) const;

  // Writes "=== REPORT: 4/6 OK, 1 WARN, 1 FAIL ===" into out. Returns chars written.
  size_t summaryLine(char* out, size_t outSize) const;

 private:
  CheckResult results_[kMaxChecks];
  size_t count_ = 0;
};

}  // namespace diag
