#include "pedronet/core/timestamp.h"
#include <sys/time.h>

namespace pedronet::core {

Timestamp Timestamp::Now() {
  struct timeval tv {};
  gettimeofday(&tv, nullptr);

  return Timestamp{tv.tv_sec * kMicroseconds + tv.tv_usec};
}
} // namespace pedronet