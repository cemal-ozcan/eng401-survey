#include "survey_logger.h"

#include <cstdio>
#include <cstring>

namespace logger {

namespace {
constexpr int kMaxFiles = 1000;
}  // namespace

void makeFileName(const char* prefix, int index, char* out, size_t size) {
  std::snprintf(out, size, "%s%03d.CSV", prefix, index);
}

SurveyLogger::SurveyLogger(hal::Storage& storage, hal::Clock& clock, const Config& cfg)
    : storage_(storage), clock_(clock), cfg_(cfg) {}

bool SurveyLogger::begin() {
  lastRetryUs_ = clock_.nowUs();
  lastFlushUs_ = lastRetryUs_;
  if (!storage_.begin()) {
    enterError();
    return false;
  }
  ok_ = openNextFile();
  if (!ok_) enterError();
  return ok_;
}

bool SurveyLogger::openNextFile() {
  storage_.close();
  // Never overwrite: skip names that already exist (earlier surveys).
  for (; nextIndex_ < kMaxFiles; ++nextIndex_) {
    makeFileName(cfg_.prefix, nextIndex_, fileName_, sizeof(fileName_));
    if (!storage_.exists(fileName_)) break;
  }
  if (nextIndex_ >= kMaxFiles || !storage_.open(fileName_)) return false;
  ++nextIndex_;
  ++stats_.filesOpened;

  const char* header = record::csvHeader();
  size_t len = std::strlen(header);
  if (storage_.write(header, len) != len || storage_.write("\n", 1) != 1 || !storage_.flush()) return false;
  fileBytes_ = static_cast<uint32_t>(len + 1);
  stats_.bytesWritten += len + 1;
  return true;
}

void SurveyLogger::enterError() {
  if (ok_) ++stats_.writeErrors;
  ok_ = false;
  lastRetryUs_ = clock_.nowUs();  // first retry one interval after the failure
  storage_.close();
}

bool SurveyLogger::writeBuffer() {
  if (bufLen_ == 0) return true;
  if (!ok_) return false;
  if (fileBytes_ + bufLen_ > cfg_.maxFileBytes && !openNextFile()) {
    enterError();
    return false;
  }
  const uint64_t t0 = clock_.nowUs();
  size_t n = storage_.write(buf_, bufLen_);
  bool flushed = (n == bufLen_) && storage_.flush();
  const uint32_t dt = static_cast<uint32_t>(clock_.nowUs() - t0);
  if (dt > stats_.maxWriteUs) stats_.maxWriteUs = dt;
  if (!flushed) {
    // A partial block may be on the card; the next file starts clean and the
    // whole block is retried there (duplicates are removable by seq number).
    enterError();
    return false;
  }
  fileBytes_ += static_cast<uint32_t>(n);
  stats_.bytesWritten += n;
  stats_.recordsWritten += bufRecords_;
  bufLen_ = 0;
  bufRecords_ = 0;
  lastFlushUs_ = clock_.nowUs();
  return true;
}

void SurveyLogger::append(const record::Record& r) {
  char line[record::kMaxCsvLen + 1];
  size_t len = record::toCsv(r, line, sizeof(line) - 1);
  if (len == 0) return;
  line[len++] = '\n';

  if (bufLen_ + len > kBufferSize) {
    writeBuffer();
    if (bufLen_ + len > kBufferSize) {  // still full: card in error state
      ++stats_.recordsDropped;
      return;
    }
  }
  std::memcpy(buf_ + bufLen_, line, len);
  bufLen_ += len;
  ++bufRecords_;
  ++stats_.recordsBuffered;
}

void SurveyLogger::tick() {
  const uint64_t now = clock_.nowUs();
  if (!ok_) {
    if (now - lastRetryUs_ >= cfg_.retryIntervalUs) {
      lastRetryUs_ = now;
      ok_ = storage_.begin() && openNextFile();
      if (ok_) writeBuffer();
    }
    return;
  }
  if (now - lastFlushUs_ >= cfg_.flushIntervalUs) writeBuffer();
}

bool SurveyLogger::flushNow() { return writeBuffer(); }

}  // namespace logger
