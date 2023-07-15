#include "pedronet/tcp_server.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

void TcpServer::Start() {
  PEDRONET_TRACE("TcpServer::Start() enter");

  acceptor_->OnAccept([this](Socket socket) {
    PEDRONET_TRACE("TcpServer::OnAccept({})", socket);
    auto connection = std::make_shared<TcpConnection>(worker_group_->Next(),
                                                      std::move(socket));

    connection->OnConnection([this](const TcpConnectionPtr& conn) {
      PEDRONET_TRACE("server raiseConnection: {}", *conn);

      std::unique_lock<std::mutex> lock(mu_);
      actives_.emplace(conn);
      lock.unlock();

      if (connection_callback_) {
        connection_callback_(conn);
      }
    });

    connection->OnClose([this](const TcpConnectionPtr& conn) {
      PEDRONET_TRACE("server disconnect: {}", *conn);

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
  PEDRONET_TRACE("TcpServer::Start() exit");
}
void TcpServer::Close() {
  PEDRONET_TRACE("TcpServer::Close() enter");
  acceptor_->Close();

  std::unique_lock<std::mutex> lock(mu_);
  for (auto& conn : actives_) {
    conn->Close();
  }
  actives_.clear();
}
void TcpServer::Bind(const pedronet::InetAddress& address) {
  PEDRONET_TRACE("TcpServer::Bind({})", address);

  if (!boss_group_) {
    PEDRONET_FATAL("boss group is not set");
  }

  acceptor_ = std::make_shared<Acceptor>(boss_group_->Next(), address,
                                         Option{});
  acceptor_->Bind();
}
}  // namespace pedronet