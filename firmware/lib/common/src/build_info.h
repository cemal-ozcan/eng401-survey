#pragma once

// Platform-independent build information. Must not include Arduino headers
// so it can be unit-tested on the host (pio test -e native).

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

#ifndef USE_MOCK
#define USE_MOCK 0
#endif

namespace build_info {

inline const char* version() { return FW_VERSION; }

inline bool isMock() { return USE_MOCK != 0; }

inline const char* modeName() { return isMock() ? "MOCK" : "REAL"; }

}  // namespace build_info
