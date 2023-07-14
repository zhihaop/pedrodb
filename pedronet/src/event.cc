#include "pedronet/event.h"
#include <poll.h>

namespace pedronet {
const SelectEvents SelectEvents::kNoneEvent{0};
const SelectEvents SelectEvents::kReadEvent{POLLIN | POLLPRI};
const SelectEvents SelectEvents::kWriteEvent{POLLOUT};

const ReceiveEvents ReceiveEvents::kHangUp{POLLHUP};
const ReceiveEvents ReceiveEvents::kInvalid{POLLNVAL};
const ReceiveEvents ReceiveEvents::kError{POLLERR};
const ReceiveEvents ReceiveEvents::kReadable{POLLIN};
const ReceiveEvents ReceiveEvents::kPriorReadable{POLLPRI};
const ReceiveEvents ReceiveEvents::kReadHangUp{POLLRDHUP};
const ReceiveEvents ReceiveEvents::kWritable{POLLOUT};
}  // namespace pedronet