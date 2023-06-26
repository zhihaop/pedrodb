#ifndef PEDRONET_TCP_SERVER_H
#define PEDRONET_TCP_SERVER_H

#include "pedronet/acceptor.h"
#include "pedronet/buffer.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inet_address.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"
#include "pedronet/tcp_connection.h"
#include <exception>
#include <memory>
#include "pedronet/core/debug.h"
#include <unordered_map>
#include <unordered_set>

namespace pedronet {

class TcpServer {
  EventLoop *acceptor_loop_{};
  std::shared_ptr<EventLoopGroup> boss_group_;
  std::shared_ptr<EventLoopGroup> worker_group_;

  std::shared_ptr<Acceptor> acceptor_;

public:
  TcpServer() = default;
  ~TcpServer() { Close(); }

  void SetGroup(const std::shared_ptr<EventLoopGroup> &boss,
                const std::shared_ptr<EventLoopGroup> &worker) {
    boss_group_ = boss;
    worker_group_ = worker;
  }

  void Bind(const InetAddress &address) {
    spdlog::error("Bind()");
    if (!boss_group_) {
      spdlog::error("boss group is not set");
      std::terminate();
    }

    if (!acceptor_loop_) {
      acceptor_loop_ = &boss_group_->Next();
    }

    acceptor_ = std::make_shared<Acceptor>(address, Acceptor::Option{});
    acceptor_->Attach(acceptor_loop_, [=] {
      spdlog::error("do bind");
      acceptor_->Bind();
    });
  }

  void Start() {
    if (!acceptor_loop_) {
      spdlog::error("bind is not invoke");
      std::terminate();
    }
    spdlog::info("starting tcp server");

    acceptor_loop_->Submit([=] {
      spdlog::info("listening");
      acceptor_->Listen([this](auto conn) {
        conn->SetMessageCallback([=](const TcpConnectionPtr &conn,
                                     Buffer *buffer, core::Timestamp now) {
          std::string buf(buffer->ReadableBytes(), 0);
          buffer->Retrieve(buf.data(), buf.size());
          if (buf.find("exit") != std::string::npos) {
            spdlog::info("try closing channel {}", conn->String());
            conn->Close();
            return;
          }
          spdlog::info("read: {}", buf.data());
          conn->Send(std::move(buf));
        });

        conn->Attach(&worker_group_->Next(), [=] {
          spdlog::info("create channel[{}]", conn->String());
          conn->EnableEvent(SelectEvents::kReadEvent);
        });
      });
    });
  }

  void Close() { acceptor_->Close(); }
};

} // namespace pedronet

#endif // PEDRONET_TCP_SERVER_H