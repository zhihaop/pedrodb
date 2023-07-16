#include "pedronet/channel/socket_channel.h"
#include "pedronet/logger/logger.h"
#include "pedronet/selector/selector.h"

namespace pedronet {

void SocketChannel::SetWritable(bool on) {
  auto ev = events_;
  if (on) {
    events_.Add(SelectEvents::kWriteEvent);
  } else {
    events_.Remove(SelectEvents::kWriteEvent);
  }
  if (ev != events_) {
    selector_->Update(this, events_);
  }
}

void SocketChannel::SetReadable(bool on) {
  auto ev = events_;
  if (on) {
    events_.Add(SelectEvents::kReadEvent);
  } else {
    events_.Remove(SelectEvents::kReadEvent);
  }
  if (ev != events_) {
    selector_->Update(this, events_);
  }
}

void SocketChannel::HandleEvents(ReceiveEvents events, Timestamp now) {
  PEDRONET_TRACE("{} handel events[{}]", *this, events.Value());
  if (events.Contains(ReceiveEvents::kHangUp) &&
      !events.Contains(ReceiveEvents::kReadable)) {
    if (close_callback_) {
      close_callback_(events, now);
    }
  }

  if (events.OneOf(ReceiveEvents::kError, ReceiveEvents::kInvalid)) {
    if (error_callback_) {
      error_callback_(events, now);
    }
  }

  if (events.OneOf(ReceiveEvents::kReadable, ReceiveEvents::kPriorReadable,
                   ReceiveEvents::kReadHangUp)) {
    if (read_callback_) {
      read_callback_(events, now);
    }
  }

  if (events.Contains(ReceiveEvents::kWritable)) {
    if (write_callback_) {
      write_callback_(events, now);
    }
  }
}
}  // namespace pedronet