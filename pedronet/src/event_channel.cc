#include "pedronet/event_channel.h"
#include <spdlog/spdlog.h>
#include <sys/eventfd.h>

namespace pedronet {

static core::File CreateEventFile() {
  int fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (fd <= 0) {
    spdlog::error("failed to create event fd, reason[{}]", errno);
    std::terminate();
  }
  return core::File{fd};
}

EventChannel::EventChannel() : core::File(CreateEventFile()), Channel() {}

void EventChannel::HandleEvents(ReceiveEvent events, core::Timestamp now) {
  uint64_t val;
  if (Read(&val, sizeof(val)) != sizeof(val)) {
    spdlog::error("failed to read event fd, errno[{}]", errno);
    std::terminate();
  }
  if (event_callback_) {
    event_callback_(events, now);
  }
}
} // namespace pedronet