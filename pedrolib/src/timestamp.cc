#include "pedrolib/timestamp.h"
#include <sys/time.h>

namespace pedrolib {

Timestamp Timestamp::Now() {
  struct timeval tv {};
  gettimeofday(&tv, nullptr);

  return Timestamp{tv.tv_sec * Duration::kMicroseconds + tv.tv_usec};
}
} // namespace pedrolib