#ifndef PEDRONET_TCP_CLIENT_H
#define PEDRONET_TCP_CLIENT_H

#include "callbacks.h"
#include "core/latch.h"
#include "event.h"
#include "pedronet/callbacks.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmoveable.h"
#include "pedronet/eventloop.h"
#include "pedronet/inet_address.h"
#include "pedronet/socket.h"
#include <memory>

#include "pedronet/tcp_connection.h"
#include "tcp_connection.h"

namespace pedronet {

class TcpClient : core::noncopyable, core::nonmoveable {

  std::shared_ptr<EventLoopGroup> worker_group_;
  InetAddress address_;
  std::shared_ptr<TcpConnection> conn_;

public:
  explicit TcpClient(const InetAddress &address) : address_(address) {}
  void SetGroup(std::shared_ptr<EventLoopGroup> worker_group) {
    worker_group_ = std::move(worker_group);
  }

  void handleConnect(Socket socket, InetAddress address) {
    if (!socket.Valid()) {
      spdlog::error("socket fd is invalid");
      return;
    }

    auto err = socket.Connect(address);
    if (!err.Empty()) {
      spdlog::error("failed to connect socket from [{}] to [{}], reason[{}]",
                    socket.GetLocalAddress(), socket.GetPeerAddress(),
                    socket.GetError());
      return;
    }
    conn_ = std::make_shared<TcpConnection>(std::move(socket));
  }

  void Start() {
    auto &loop = worker_group_->Next();
    spdlog::info("tcp client start, connecting, {}, {}", address_,
                 address_.Family());
    core::Latch latch(1);
    loop.Submit([=, &latch] {
      handleConnect(Socket::Create(address_.Family()), address_);
      latch.CountDown();
    });
    latch.Await();
    conn_->Attach(&loop, [=] {
      conn_->EnableEvent(SelectEvents::kNoneEvent);
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