#include "pedronet/channel.h"

#include <poll.h>

namespace pedronet {

const ReceiveEvent Channel::kHangUp = POLLHUP;
const ReceiveEvent Channel::kInvalid = POLLNVAL;
const ReceiveEvent Channel::kError = POLLERR;
const ReceiveEvent Channel::kReadable = POLLIN;
const ReceiveEvent Channel::kPriorReadable = POLLPRI;
const ReceiveEvent Channel::kReadHangUp = POLLRDHUP;
const ReceiveEvent Channel::kWritable = POLLOUT;

} // namespace pedronet
