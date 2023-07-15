#include "pedronet/channel/event_channel.h"
#include <sys/eventfd.h>
#include "pedronet/logger/logger.h"

namespace pedronet {

static File CreateEventFile() {
  int fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (fd <= 0) {
    PEDRONET_FATAL("failed to create event fd, reason[{}]", errno);
  }
  return File{fd};
}

EventChannel::EventChannel() : Channel(), file_(CreateEventFile()) {}

void EventChannel::HandleEvents(ReceiveEvents, Timestamp) {
  uint64_t val;
  if (file_.Read(&val, sizeof(val)) != sizeof(val)) {
    PEDRONET_FATAL("failed to read event fd: ", file_.GetError());
  }

  if (event_callback_) {
    event_callback_();
  }
}

std::string EventChannel::String() const {
  return fmt::format("EventChannel[fd={}]", file_.Descriptor());
}

void EventChannel::WakeUp() {
  uint64_t val = 1;
  file_.Write(&val, sizeof(val));
}
}  // namespace pedronet