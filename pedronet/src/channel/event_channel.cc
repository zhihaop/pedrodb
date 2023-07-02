#include "pedronet/channel/event_channel.h"
#include "pedronet/logger/logger.h"
#include <sys/eventfd.h>

namespace pedronet {

static core::File CreateEventFile() {
  int fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (fd <= 0) {
    PEDRONET_FATAL("failed to create event fd, reason[{}]", errno);
  }
  return core::File{fd};
}

EventChannel::EventChannel() : Channel(), file_(CreateEventFile()) {}

void EventChannel::HandleEvents(ReceiveEvents events, Timestamp now) {
  uint64_t val;
  if (file_.Read(&val, sizeof(val)) != sizeof(val)) {
    PEDRONET_FATAL("failed to read event fd: ", file_.GetError());
  }
  if (event_callback_) {
    event_callback_(events, now);
  }
}

std::string EventChannel::String() const {
  return fmt::format("EventChannel[fd={}]", file_.Descriptor());
}
} // namespace pedronet