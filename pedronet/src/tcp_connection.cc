#include "pedronet/tcp_connection.h"

namespace pedronet {
void TcpConnection::Start() {
  eventloop_.Register(&channel_, [conn = shared_from_this()] {
    switch (conn->state_) {
    case State::kConnecting: {
      conn->state_ = State::kConnected;
      conn->channel_.SetReadable(true);
      if (conn->connection_callback_) {
        conn->connection_callback_(conn);
      }
      break;
    }
    case State::kDisconnecting: {
      conn->state_ = State::kDisconnected;
      if (conn->close_callback_) {
        conn->close_callback_(conn);
      }
      break;
    }
    default: {
      spdlog::warn("TcpConnection::RegisterCallback(): should not happened");
      break;
    }
    }
  });
}
void TcpConnection::Send(const std::string &data) {
  eventloop_.AssertInsideLoop();
  
  if (state_ != State::kConnected) {
    spdlog::trace("ignore sending data[{}]", data);
    return;
  }

  if (state_ == State::kDisconnected) {
    spdlog::warn("give up sending");
    return;
  }

  ssize_t w0 = TrySendingDirect(data);
  if (w0 < 0) {
    auto err = channel_.GetError();
    if (err.GetCode() != EWOULDBLOCK) {
      spdlog::error("{}::HandleSend throws {}", err.GetReason());
      return;
    }
  }

  output_->EnsureWriteable(data.size() - w0);
  size_t w1 = output_->Append(data.data() + w0, data.size() - w0);
  // TODO: high watermark
  if (w1 != 0) {
    channel_.SetWritable(true);
  }
}
ssize_t TcpConnection::TrySendingDirect(const std::string &data) {
  eventloop_.AssertInsideLoop();
  if (channel_.Writable()) {
    return 0;
  }
  if (output_->ReadableBytes() != 0) {
    return 0;
  }
  return channel_.Write(data.data(), data.size());
}
void TcpConnection::HandleRead(ReceiveEvents events, core::Timestamp now) {
  input_->EnsureWriteable(1024);
  ssize_t n = input_->Append(&channel_, input_->WritableBytes());
  spdlog::trace("read {} bytes", n);
  if (n < 0) {
    HandleError(events, now);
    return;
  }

  if (n == 0) {
    Close();
    return;
  }

  if (message_callback_) {
    message_callback_(shared_from_this(), input_.get(), now);
  }
}
void TcpConnection::HandleError(ReceiveEvents events, core::Timestamp now) {
  auto err = channel_.GetError();
  if (error_callback_) {
    error_callback_(shared_from_this());
  }
  spdlog::error("TcpConnection error, reason[{}]", err.GetReason());
}
void TcpConnection::WriteBuffer(ReceiveEvents events, core::Timestamp now) {
  if (!channel_.Writable()) {
    spdlog::trace("{} is down, no more writing", *this);
    return;
  }

  ssize_t n = output_->Retrieve(&channel_, output_->ReadableBytes());
  if (n < 0) {
    auto err = channel_.GetError();
    spdlog::error("failed to write socket, reason[{}]", err.GetReason());
    return;
  }
  if (output_->ReadableBytes() == 0) {
    channel_.SetWritable(false);
    if (write_complete_callback_) {
      eventloop_.Submit([connection = shared_from_this()] {
        connection->write_complete_callback_(connection);
      });
    }

    if (state_ == State::kDisconnecting) {
      channel_.CloseWrite();
    }
  }
}
void TcpConnection::Close() {
  if (state_ == State::kConnected) {
    state_ = State::kDisconnecting;
  }

  if (state_ == State::kDisconnecting) {
    if (input_->ReadableBytes() == 0 && output_->ReadableBytes() == 0) {
      spdlog::trace("::Close()", *this);
      channel_.SetWritable(false);
      channel_.SetReadable(false);
      eventloop_.Deregister(&channel_);
    }
  }
}
std::string TcpConnection::String() const {
  return fmt::format("TcpConnection[local={}, peer={}, channel={}]", local_,
                     peer_, channel_);
}
TcpConnection::TcpConnection(EventLoop &eventloop, Socket socket)
    : channel_(std::move(socket)), local_(channel_.GetLocalAddress()),
      peer_(channel_.GetPeerAddress()), eventloop_(eventloop) {

  channel_.OnRead(
      [this](auto events, auto now) { return HandleRead(events, now); });
  channel_.OnWrite(
      [this](auto events, auto now) { return WriteBuffer(events, now); });
  channel_.OnClose([this](auto events, auto now) { return Close(); });
  channel_.OnError(
      [this](auto events, auto now) { return HandleError(events, now); });

  channel_.SetSelector(eventloop.GetSelector());
}
TcpConnection::~TcpConnection() {
  Close();
  spdlog::trace("{}::~TcpConnection()", *this);
}
} // namespace pedronet
