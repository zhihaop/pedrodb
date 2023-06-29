#ifndef PEDRONET_TCP_CLIENT_H
#define PEDRONET_TCP_CLIENT_H

#include "callbacks.h"
#include "core/latch.h"
#include "event.h"
#include "pedronet/callbacks.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"
#include "pedronet/eventloop.h"
#include "pedronet/inetaddress.h"
#include "pedronet/socket.h"
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

#include "pedronet/tcp_connection.h"
#include "socket.h"
#include "tcp_connection.h"

namespace pedronet {

class TcpClient : core::noncopyable, core::nonmovable {

  std::shared_ptr<EventLoopGroup> worker_group_;
  InetAddress address_;
  std::shared_ptr<TcpConnection> conn_;

public:
  explicit TcpClient(InetAddress address) : address_(std::move(address)) {}
  void SetGroup(std::shared_ptr<EventLoopGroup> worker_group) {
    worker_group_ = std::move(worker_group);
  }

  void connecting(Socket socket) {}

  void retry(Socket socket) {}

  void connect(const InetAddress& address) {
    Socket socket = Socket::Create(address.Family());
    if (!socket.Valid()) {
      spdlog::error("socket fd is invalid");
      return;
    }

    auto err = socket.Connect(address);
    switch (err.GetCode()) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting(std::move(socket));
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(std::move(socket));
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      spdlog::error("connect error: {}", err);
      break;

    default:
      spdlog::error("unexpected connect error: {}", err);
      break;
    }
  }

  void Start() {
    auto &loop = worker_group_->Next();
    spdlog::info("tcp client start, connecting, {}, {}", address_,
                 address_.Family());
    core::Latch latch(1);
    loop.Submit([=, &latch] {
      connect(address_);
      latch.CountDown();
    });
    latch.Await();

    conn_->OnConnection([=](auto conn) {
      conn_->Send("hello world");
      spdlog::info("send hello world");
    });
    
  }

  void Close() {
    if (conn_) {
      conn_->Close();
    }
  }
};
} // namespace pedronet

#endif // PEDRONET_TCP_CLIENT_H