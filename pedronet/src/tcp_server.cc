#include "pedronet/tcp_server.h"

namespace pedronet {
void TcpServer::Start() {
  spdlog::trace("TcpServer::Start() enter");

  acceptor_->OnAccept([this](Socket socket) {
    spdlog::trace("TcpServer::OnAccept({})", socket);
    auto connection = std::make_shared<TcpConnection>(worker_group_->Next(),
                                                      std::move(socket));

    connection->OnConnection([this](const TcpConnectionPtr &conn) {
      spdlog::trace("server connect: {}", *conn);

      std::unique_lock<std::mutex> lock(mu_);
      actives_.emplace(conn);
      lock.unlock();

      if (connection_callback_) {
        connection_callback_(conn);
      }
    });

    connection->OnClose([this](const TcpConnectionPtr &conn) {
      spdlog::trace("server disconnect: {}", *conn);

      std::unique_lock<std::mutex> lock(mu_);
      actives_.erase(conn);
      lock.unlock();

      if (close_callback_) {
        close_callback_(conn);
      }
    });

    connection->OnMessage(message_callback_);
    connection->OnError(error_callback_);
    connection->OnWriteComplete(write_complete_callback_);
    connection->OnHighWatermark(high_watermark_callback_);
    connection->Start();
  });

  acceptor_->Listen();
  spdlog::trace("TcpServer::Start() exit");
}
void TcpServer::Close() {
  spdlog::trace("TcpServer::Close() enter");
  acceptor_->Close();

  std::unique_lock<std::mutex> lock(mu_);
  for (auto &conn : actives_) {
    conn->Close();
  }
  actives_.clear();
}
void TcpServer::Bind(const pedronet::InetAddress &address) {
  spdlog::trace("TcpServer::Bind({})", address);

  if (!boss_group_) {
    spdlog::error("boss group is not set");
    std::terminate();
  }

  acceptor_ = std::make_shared<Acceptor>(boss_group_->Next(), address,
                                         Acceptor::Option{});
  acceptor_->Bind();
}
} // namespace pedronet