#include "hal.h"

namespace hal {

const char* statusText(Status s) {
  switch (s) {
    case Status::Ok: return "OK";
    case Status::NotReady: return "NOT_READY";
    case Status::NotFound: return "NOT_FOUND";
    case Status::ReadError: return "READ_ERROR";
  }
  return "?";
}

}  // namespace hal
