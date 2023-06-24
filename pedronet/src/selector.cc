#include "pedronet/selector.h"
#include <poll.h>
namespace pedronet {

const SelectorEvents Selector::kNoneEvent{0};
const SelectorEvents Selector::kReadEvent{POLLIN | POLLPRI};
const SelectorEvents Selector::kWriteEvent{POLLOUT};
} // namespace pedronet