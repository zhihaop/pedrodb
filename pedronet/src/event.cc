#include "pedronet/event.h"
#include <poll.h>
#include <sys/epoll.h>

namespace pedronet {
const SelectEvents SelectEvents::kNoneEvent{0};
const SelectEvents SelectEvents::kReadEvent{POLLIN | POLLPRI};
const SelectEvents SelectEvents::kWriteEvent{POLLOUT};

SelectEvents SelectEvents::Trigger(SelectTrigger trigger) const noexcept {
  SelectEvents events = *this;
  if (trigger == SelectTrigger::kEdge) {
    events.Add(SelectEvents{EPOLLET});
  }
  return events;
}

const ReceiveEvents ReceiveEvents::kHangUp{POLLHUP};
const ReceiveEvents ReceiveEvents::kInvalid{POLLNVAL};
const ReceiveEvents ReceiveEvents::kError{POLLERR};
const ReceiveEvents ReceiveEvents::kReadable{POLLIN};
const ReceiveEvents ReceiveEvents::kPriorReadable{POLLPRI};
const ReceiveEvents ReceiveEvents::kReadHangUp{POLLRDHUP};
const ReceiveEvents ReceiveEvents::kWritable{POLLOUT};
}  // namespace pedronet