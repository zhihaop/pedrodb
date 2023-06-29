#include "pedronet/tcp_client.h"

void pedronet::TcpClient::connecting(pedronet::EventLoop &loop,
                                     pedronet::Socket socket) {
  spdlog::trace("TcpClient::connection({})", socket);
  auto connection = std::make_shared<TcpConnection>(loop, std::move(socket));

  connection->OnClose([this](const TcpConnectionPtr &conn) {
    spdlog::trace("client disconnect: {}", *conn);

    std::unique_lock<std::mutex> lock(mu_);
    actives_.erase(conn);
    lock.unlock();

    if (close_callback_) {
      close_callback_(conn);
    }
  });

  connection->OnConnection([this](const TcpConnectionPtr &connection) {
    spdlog::trace("client connect: {}", *connection);

    std::unique_lock<std::mutex> lock(mu_);
    actives_.emplace(connection);
    lock.unlock();

    if (connection_callback_) {
      connection_callback_(connection);
    }
  });

  connection->OnClose(close_callback_);
  connection->OnError(error_callback_);
  connection->OnWriteComplete(write_complete_callback_);
  connection->OnMessage(message_callback_);
  connection->Start();
}

void pedronet::TcpClient::connect(pedronet::EventLoop &loop) {
  Socket socket = Socket::Create(address_.Family());
  if (!socket.Valid()) {
    spdlog::error("socket fd is invalid");
    return;
  }

  auto err = socket.Connect(address_);
  switch (err.GetCode()) {
  case 0:
  case EINPROGRESS:
  case EINTR:
  case EISCONN:
    connecting(loop, std::move(socket));
    break;

  case EAGAIN:
  case EADDRINUSE:
  case EADDRNOTAVAIL:
  case ECONNREFUSED:
  case ENETUNREACH:
    retry(loop, std::move(socket), err);
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
void pedronet::TcpClient::retry(pedronet::EventLoop &loop,
                                pedronet::Socket socket,
                                pedronet::core::File::Error reason) {
  socket.Close();
  spdlog::trace("TcpClient::retry(): {}", reason);
  loop.ScheduleAfter([&] { connect(loop); }, core::Duration::Seconds(1));
}

void pedronet::TcpClient::Start() {
  spdlog::trace("TcpClient::Start()");
  auto &loop = worker_group_->Next();
  loop.Submit([this, &loop] { connect(loop); });
}

void pedronet::TcpClient::Close() {
  std::unique_lock<std::mutex> lock(mu_);
  for (auto &conn : actives_) {
    if (conn == nullptr) {
      continue;
    }
    conn->Close();
  }
  actives_.clear();
}
