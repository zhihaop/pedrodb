#include "pedronet/core/timestamp.h"
#include <sys/time.h>

namespace pedronet {
namespace core {

Timestamp Timestamp::Now() {
  struct timeval tv {};
  gettimeofday(&tv, nullptr);

  return {tv.tv_sec * kMicroseconds + tv.tv_usec};
}
} // namespace core
} // namespace pedronet