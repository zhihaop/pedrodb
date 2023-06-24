#include "pedronet/timer_channel.h"

#include <bits/types/struct_itimerspec.h>
#include <spdlog/spdlog.h>
#include <sys/timerfd.h>

namespace pedronet {

inline static core::File CreateTimerFile() {
  int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd <= 0) {
    spdlog::error("failed to create timer fd");
    std::terminate();
  }
  return core::File{fd};
}

TimerChannel::TimerChannel() : core::File(CreateTimerFile()), Channel() {}

void TimerChannel::HandleEvents(ReceiveEvent events, core::Timestamp now) {
  uint64_t val;
  if (Read(&val, sizeof(val)) != sizeof(val)) {
    spdlog::error("failed to read event fd, errno[{}]", errno);
    std::terminate();
  }
  if (event_callback_) {
    event_callback_(events, now);
  }
}

void TimerChannel::WakeUpAfter(core::Duration duration) {
  duration = std::max(duration, kMinWakeUpDuration);

  struct itimerspec u {};
  struct itimerspec v {};
  int64_t usec = duration.Microseconds();
  v.it_value.tv_sec = usec / core::kMicroseconds;
  v.it_value.tv_nsec = (usec % core::kMicroseconds) * 1000;

  if (::timerfd_settime(Descriptor(), 0, &v, &u) < 0) {
    spdlog::error("failed to set timerfd time");
    std::terminate();
  }
}
} // namespace pedronet