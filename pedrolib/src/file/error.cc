#include "pedrolib/file/error.h"

namespace pedrolib {
const char *Error::GetReason() const noexcept {
  thread_local char buf[1024];
  return strerror_r(code_, buf, sizeof(buf));
}

} // namespace pedrolib