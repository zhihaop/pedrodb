#ifndef PEDRONET_CHANNEL_SOCKET_CHANNEL_H
#define PEDRONET_CHANNEL_SOCKET_CHANNEL_H

#include "pedronet/channel/channel.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/latch.h"
#include "pedronet/eventloop.h"
#include "pedronet/socket.h"

namespace pedronet {

class SocketChannel final : public Channel {
protected:
  SelectEvents events_{SelectEvents::kNoneEvent};

  SelectorCallback close_callback_;
  SelectorCallback read_callback_;
  SelectorCallback error_callback_;
  SelectorCallback write_callback_;
  EventUpdateCallback event_update_callback_;
  Socket &socket_;

public:
  explicit SocketChannel(Socket &socket) : socket_(socket) {}

  ~SocketChannel() override { Close(); }

  void Close() {
    spdlog::info("SocketChannel::Close() enter");
    SetReadable(false);
    SetWritable(false);
    event_update_callback_ = {};
  }

  void OnRead(SelectorCallback read_callback) {
    read_callback_ = std::move(read_callback);
  }

  void OnEventUpdate(EventUpdateCallback event_update_callback) {
    event_update_callback_ = std::move(event_update_callback);
  }

  void OnClose(SelectorCallback close_callback) {
    close_callback_ = std::move(close_callback);
  }

  void OnWrite(SelectorCallback write_callback) {
    write_callback_ = std::move(write_callback);
  }

  void OnError(SelectorCallback error_callback) {
    error_callback_ = std::move(error_callback);
  }

  void HandleEvents(ReceiveEvents events, core::Timestamp now) final {
    spdlog::info("{} handel events[{}]", String(), events.Value());
    if (events.Contains(ReceiveEvents::kHangUp) &&
        !events.Contains(ReceiveEvents::kReadable)) {
      if (close_callback_) {
        close_callback_(events, now);
      }
    }

    if (events.OneOf({ReceiveEvents::kError, ReceiveEvents::kInvalid})) {
      if (error_callback_) {
        error_callback_(events, now);
      }
    }

    if (events.OneOf({ReceiveEvents::kReadable, ReceiveEvents::kPriorReadable,
                      ReceiveEvents::kReadHangUp})) {
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

  void SetReadable(bool on) {
    if (on) {
      events_.Add(SelectEvents::kReadEvent);
    } else {
      events_.Remove(SelectEvents::kReadEvent);
    }
    if (event_update_callback_) {
      event_update_callback_(events_);
    }
  }

  bool Readable() const noexcept {
    return events_.Contains(SelectEvents::kReadEvent);
  }

  bool Writable() const noexcept {
    return events_.Contains(SelectEvents::kWriteEvent);
  }

  void SetWritable(bool on) {
    if (on) {
      events_.Add(SelectEvents::kWriteEvent);
    } else {
      events_.Remove(SelectEvents::kWriteEvent);
    }
    if (event_update_callback_) {
      event_update_callback_(events_);
    }
  }

  Socket &File() noexcept final { return socket_; }
  const Socket &File() const noexcept final { return socket_; }
};

} // namespace pedronet
#endif // PEDRONET_CHANNEL_SOCKET_CHANNEL_H