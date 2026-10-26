#pragma once

// In-memory storage used in mock mode and tests. Can simulate a full card or
// a write failure (e.g. card pulled out).

#include <cstdint>
#include <map>
#include <string>

#include "hal.h"

namespace hal {

class MockStorage : public Storage {
 public:
  explicit MockStorage(size_t capacityBytes = 64 * 1024) : capacity_(capacityBytes) {}

  bool begin() override { return present_; }
  bool exists(const char* path) override { return files_.count(path) != 0; }
  bool open(const char* path) override;
  size_t write(const char* data, size_t len) override;
  bool flush() override { return present_ && !current_.empty(); }
  void close() override { current_.clear(); }
  uint64_t freeBytes() override { return capacity_ - used_; }

  // Test / fault-injection helpers
  void setPresent(bool present) { present_ = present; }
  const std::string& contents(const std::string& path) { return files_[path]; }
  size_t fileCount() const { return files_.size(); }

 private:
  std::map<std::string, std::string> files_;
  std::string current_;
  size_t capacity_;
  size_t used_ = 0;
  bool present_ = true;
};

}  // namespace hal
