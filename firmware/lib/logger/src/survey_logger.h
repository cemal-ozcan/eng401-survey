#pragma once

// Buffered CSV logger (requirement R6).
//
// - Records are encoded into a RAM buffer and written to the card in blocks;
//   SD cards are fastest with large writes and every write costs wear.
// - The buffer is flushed when it is nearly full or every flushIntervalUs, so a
//   power cut loses at most that many seconds of data.
// - Each boot creates a new file /SURVEY_NNN.CSV (header included); a file is
//   rotated when it reaches maxFileBytes.
// - On a write error (card pulled, full) the logger keeps buffering, retries
//   with a fresh file every retryIntervalUs and counts records it had to drop.

#include <cstddef>
#include <cstdint>

#include "hal.h"
#include "record.h"

namespace logger {

struct Config {
  uint64_t flushIntervalUs = 5000000;    // 5 s
  uint64_t retryIntervalUs = 5000000;
  uint32_t maxFileBytes = 16u * 1024u * 1024u;
  const char* prefix = "/SURVEY_";       // -> /SURVEY_000.CSV ... /SURVEY_999.CSV
};

struct Stats {
  uint32_t recordsBuffered = 0;  // accepted into the buffer
  uint32_t recordsWritten = 0;   // confirmed on the card
  uint32_t recordsDropped = 0;   // lost because the buffer was full during an error
  uint32_t writeErrors = 0;
  uint32_t filesOpened = 0;
  uint64_t bytesWritten = 0;
  uint32_t maxWriteUs = 0;       // longest single block write (timing analysis)
};

class SurveyLogger {
 public:
  static constexpr size_t kBufferSize = 4096;

  SurveyLogger(hal::Storage& storage, hal::Clock& clock, const Config& cfg = Config());

  // Mounts the card and opens a new file. Returns false if that failed (the
  // logger will keep retrying from tick()).
  bool begin();

  // Buffers one record (writes a block if the buffer is full).
  void append(const record::Record& r);

  // Call regularly (also when no records arrive): time-based flush and retries.
  void tick();

  // Write everything buffered now (e.g. before a planned power-off).
  bool flushNow();

  bool ok() const { return ok_; }
  const char* fileName() const { return fileName_; }
  const Stats& stats() const { return stats_; }

 private:
  bool openNextFile();
  bool writeBuffer();
  void enterError();

  hal::Storage& storage_;
  hal::Clock& clock_;
  Config cfg_;
  Stats stats_;
  bool ok_ = false;
  char fileName_[32] = "";
  int nextIndex_ = 0;
  uint32_t fileBytes_ = 0;
  uint64_t lastFlushUs_ = 0;
  uint64_t lastRetryUs_ = 0;

  char buf_[kBufferSize];
  size_t bufLen_ = 0;
  uint32_t bufRecords_ = 0;  // records currently in buf_
};

// Builds "/SURVEY_007.CSV" into out. Exposed for tests.
void makeFileName(const char* prefix, int index, char* out, size_t size);

}  // namespace logger
