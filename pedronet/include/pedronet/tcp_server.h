#ifndef PEDRONET_TCP_SERVER_H
#define PEDRONET_TCP_SERVER_H

#include "callbacks.h"
#include "pedronet/acceptor.h"
#include "pedronet/buffer.h"
#include "pedronet/core/debug.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inetaddress.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"
#include "pedronet/tcp_connection.h"
#include <exception>
#include <memory>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <unordered_set>

namespace pedronet {

class TcpServer {
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
    spdlog::info("TcpServer::Bind({})", address);

    if (!boss_group_) {
      spdlog::error("boss group is not set");
      std::terminate();
    }

    acceptor_ = std::make_shared<Acceptor>(boss_group_->Next(), address,
                                           Acceptor::Option{});
    acceptor_->Start();
    acceptor_->Bind();
  }

  void Start() {
    spdlog::info("TcpServer::Start() enter");

    acceptor_->OnAccept([this](Socket socket) {
      spdlog::info("TcpServer::OnAccept({})", socket);
      auto connection = std::make_shared<TcpConnection>(worker_group_->Next(),
                                                        std::move(socket));
      
      connection->OnConnection([] (const TcpConnectionPtr& conn) {
        spdlog::info("{}::HandleConnection()", *conn);
      });
      connection->OnMessage([&](const TcpConnectionPtr &conn, Buffer *buffer,
                                core::Timestamp now) {
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
      
      connection->Start();
    });

    acceptor_->Listen();
    spdlog::info("TcpServer::Start() exit");
  }

  void Close() {
    spdlog::info("TcpServer::Close() enter");
    acceptor_->Close();
  }
};

} // namespace pedronet

#endif // PEDRONET_TCP_SERVER_H