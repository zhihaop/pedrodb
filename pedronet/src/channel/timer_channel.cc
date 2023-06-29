#include "pedronet/channel/timer_channel.h"

#include "pedronet/core/debug.h"
#include <bits/types/struct_itimerspec.h>
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

TimerChannel::TimerChannel() : Channel(), file_(CreateTimerFile()) {}

void TimerChannel::HandleEvents(ReceiveEvents events, core::Timestamp now) {
  uint64_t val;
  if (file_.Read(&val, sizeof(val)) != sizeof(val)) {
    spdlog::error("failed to read timer fd: {}", file_.GetError());
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

  if (::timerfd_settime(file_.Descriptor(), 0, &v, &u) < 0) {
    spdlog::error("failed to set timerfd time");
    std::terminate();
  }
}

std::string TimerChannel::String() const {
  return fmt::format("TimerChannel[fd={}]", file_.Descriptor());
}
} // namespace pedronet