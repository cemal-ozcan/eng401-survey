#include "mock_storage.h"

namespace hal {

bool MockStorage::open(const char* path) {
  if (!present_) return false;
  current_ = path;
  files_[current_];  // create if missing
  return true;
}

size_t MockStorage::write(const char* data, size_t len) {
  if (!present_ || current_.empty()) return 0;
  size_t room = capacity_ - used_;
  size_t n = len < room ? len : room;  // short write when full, like a real FS
  files_[current_].append(data, n);
  used_ += n;
  return n;
}

}  // namespace hal
