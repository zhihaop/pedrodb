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

  enum class State {
    kListening,
    kClosed,
  };

protected:
  AcceptorCallback acceptor_callback_;
  State state_{Acceptor::State::kClosed};
  InetAddress address_;

  Socket socket_;
  SocketChannel channel_;
  EventLoop &eventloop_;

public:
  Acceptor(EventLoop &eventloop, const InetAddress &address,
           const Option &option)
      : socket_(Socket::Create(address.Family())), address_(address),
        channel_(socket_), eventloop_(eventloop) {
    spdlog::error("create acceptor");

    socket_.SetReuseAddr(option.reuse_addr);
    socket_.SetReusePort(option.reuse_port);
    socket_.SetKeepAlive(option.keep_alive);
    socket_.SetTcpNoDelay(option.tcp_no_delay);
    
    channel_.OnEventUpdate(
        [this](SelectEvents events) { eventloop_.Update(&channel_, events); });

    channel_.OnRead([this](auto events, auto now) {
      while (!eventloop_.Closed()) {
        spdlog::info("{}::HandleRead()", *this);
        Socket socket;
        Socket::Error err = socket_.Accept(address_, &socket);
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
  
  void Start() {
    eventloop_.Register(&channel_, [this] {
      spdlog::info("{}::Start() finished", *this);
    });
  }

  const InetAddress &ListenAddress() const noexcept { return address_; }

  void Bind() { socket_.Bind(address_); }

  void OnAccept(AcceptorCallback acceptor_callback) {
    acceptor_callback_ = std::move(acceptor_callback);
  }

  void Listen() {
    eventloop_.Submit([this] {
      state_ = State::kListening;
      channel_.SetReadable(true);
      socket_.Listen();
    });
  }

  State GetState() const noexcept { return state_; }

  void Close() {
    spdlog::info("Acceptor::Close() enter");
    core::Latch latch(1);
    eventloop_.Submit([&] {
      channel_.SetReadable(false);
      channel_.SetWritable(false);
      channel_.Close();
      latch.CountDown();
    });
    latch.Await();
    spdlog::info("Acceptor::Close() exit");
  }

  std::string String() const {
    return fmt::format("Acceptor[socket={}]", socket_);
  }
};
} // namespace pedronet

PEDRONET_FORMATABLE_CLASS(pedronet::Acceptor)

#endif // PEDRONET_ACCEPTOR_H