#include "pedronet/tcp_client.h"
#include "pedronet/logger/logger.h"

using pedronet::Duration;

namespace pedronet {
void TcpClient::handleConnection(Socket socket) {
  State s = State::kConnecting;
  if (!state_.compare_exchange_strong(s, State::kConnected)) {
    PEDRONET_WARN("state_ != State::kConnection, connection closed");
    return;
  }

  connection_ = std::make_shared<TcpConnection>(*eventloop_, std::move(socket));

  connection_->OnClose([this](auto&& conn) {
    PEDRONET_TRACE("client disconnect: {}", *conn);
    connection_.reset();

    state_ = State::kDisconnected;

    if (close_callback_) {
      close_callback_(conn);
    }
  });

  connection_->OnConnection([this](auto&& conn) {
    if (connection_callback_) {
      connection_callback_(conn);
    }
  });

  connection_->OnError(std::move(error_callback_));
  connection_->OnWriteComplete(std::move(write_complete_callback_));
  connection_->OnMessage(std::move(message_callback_));
  connection_->Start();
}

void TcpClient::raiseConnection() {
  if (state_ != State::kConnecting) {
    PEDRONET_WARN("TcpClient::raiseConnection() state is not kConnecting");
    return;
  }

  Socket socket = Socket::Create(address_.Family(), true);
  socket.SetKeepAlive(true);
  socket.SetReuseAddr(true);
  socket.SetReusePort(true);
  socket.SetTcpNoDelay(true);

  auto err = socket.Connect(address_);
  switch (err.GetCode()) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      handleConnection(std::move(socket));
      return;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(std::move(socket), err);
      return;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      PEDRONET_ERROR("raiseConnection error: {}", err);
      break;

    default:
      PEDRONET_ERROR("unexpected raiseConnection error: {}", err);
      break;
  }

  state_ = State::kOffline;
}

void TcpClient::retry(Socket socket, Error reason) {
  socket.Close();
  PEDRONET_TRACE("TcpClient::retry(): {}", reason);
  eventloop_->ScheduleAfter(Duration::Seconds(1), [&] { raiseConnection(); });
}

void TcpClient::Start() {
  PEDRONET_TRACE("TcpClient::Start()");

  State s = State::kOffline;
  if (!state_.compare_exchange_strong(s, State::kConnecting)) {
    PEDRONET_WARN("TcpClient::Start() has been invoked");
    return;
  }

  eventloop_ = &worker_group_->Next();
  eventloop_->Run([this] { raiseConnection(); });
}

void TcpClient::Close() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (connection_) {
    connection_->Close();
  }
}

void TcpClient::ForceClose() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (connection_) {
    connection_->ForceClose();
  }
}
void TcpClient::Shutdown() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (connection_) {
    connection_->Shutdown();
  }
}
void TcpClient::ForceShutdown() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  if (connection_) {
    connection_->ForceShutdown();
  }
}
}  // namespace pedronet