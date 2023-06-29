#ifndef PEDRONET_ACCEPTOR_H
#define PEDRONET_ACCEPTOR_H

#include "callbacks.h"
#include "channel/socket_channel.h"
#include "core/noncopyable.h"
#include "core/nonmovable.h"
#include "event.h"
#include "eventloop.h"
#include "pedronet/channel/socket_channel.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/latch.h"
#include "pedronet/event.h"
#include "pedronet/socket.h"
#include "socket.h"

#include "pedronet/core/debug.h"
#include <algorithm>
#include <functional>
#include <spdlog/spdlog.h>

namespace pedronet {

using AcceptorCallback = std::function<void(Socket)>;

class Acceptor : core::noncopyable, core::nonmovable {
public:
  struct Option {
    bool reuse_addr = true;
    bool reuse_port = true;
    bool keep_alive = true;
    bool tcp_no_delay = true;
  };

protected:
  AcceptorCallback acceptor_callback_;
  InetAddress address_;

  SocketChannel channel_;
  EventLoop &eventloop_;

public:
  Acceptor(EventLoop &eventloop, const InetAddress &address,
           const Option &option)
      : address_(address), channel_(Socket::Create(address.Family())),
        eventloop_(eventloop) {
    spdlog::trace("Acceptor::Acceptor()");

    auto &socket = channel_.File();
    socket.SetReuseAddr(option.reuse_addr);
    socket.SetReusePort(option.reuse_port);
    socket.SetKeepAlive(option.keep_alive);
    socket.SetTcpNoDelay(option.tcp_no_delay);

    channel_.SetSelector(eventloop.GetSelector());

    channel_.OnRead([this](auto events, auto now) {
      while (true) {
        spdlog::trace("{}::HandleRead()", *this);
        Socket socket;
        Socket::Error err = channel_.File().Accept(address_, &socket);
        if (!err.Empty()) {
          if (err.GetCode() == EAGAIN || err.GetCode() == EWOULDBLOCK) {
            break;
          }
          spdlog::error("failed to accept [{}]", err);
          continue;
        }
        if (acceptor_callback_) {
          acceptor_callback_(std::move(socket));
        }
      }
    });
  }

  ~Acceptor() { Close(); }

  void Bind() { channel_.File().Bind(address_); }

  void OnAccept(AcceptorCallback acceptor_callback) {
    acceptor_callback_ = std::move(acceptor_callback);
  }

  void Listen() {
    eventloop_.Register(&channel_, [this] {
      channel_.SetReadable(true);
      channel_.File().Listen();
    });
  }

  void Close() {
    spdlog::trace("Acceptor::Close() enter");
    core::Latch latch(1);
    eventloop_.Submit([this, &latch] {
      channel_.SetReadable(false);
      channel_.SetWritable(false);
      eventloop_.Deregister(&channel_);
      latch.CountDown();
    });
    latch.Await();
    spdlog::trace("Acceptor::Close() exit");
  }

  std::string String() const {
    return fmt::format("Acceptor[socket={}]", channel_.File());
  }
};
} // namespace pedronet

PEDRONET_CLASS_FORMATTER(pedronet::Acceptor)

#endif // PEDRONET_ACCEPTOR_H