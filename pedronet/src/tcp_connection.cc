#include "pedronet/tcp_connection.h"

namespace pedronet {
void TcpConnection::Start() {
  eventloop_.Register(&channel_, [conn = shared_from_this()] {
    switch (conn->state_) {
    case State::kConnecting: {
      PEDRONET_INFO("handleConnection {}", *conn);
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
      PEDRONET_WARN("TcpConnection::RegisterCallback(): should not happened");
      break;
    }
    }
  });
}

void TcpConnection::handleRead(core::Timestamp now) {
  ssize_t n = input_.Append(&channel_.File());
  PEDRONET_TRACE("read {} bytes", n);
  if (n < 0) {
    handleError(channel_.GetError());
    return;
  }

  if (n == 0) {
    Close();
    return;
  }

  if (message_callback_) {
    message_callback_(shared_from_this(), input_, now);
  }
}
void TcpConnection::handleError(Socket::Error err) {
  if (err.Empty()) {
    ForceClose();
    return;
  }
  
  if (error_callback_) {
    error_callback_(shared_from_this(), err);
    return;
  }
  PEDRONET_ERROR("{}::handleError(): [{}]", *this, err);
}
void TcpConnection::handleWrite() {
  if (!channel_.Writable()) {
    PEDRONET_TRACE("{} is down, no more writing", *this);
    return;
  }

  ssize_t n = output_.Retrieve(&channel_.File());
  if (n < 0) {
    handleError(channel_.GetError());
    return;
  }

  if (output_.ReadableBytes() == 0) {
    channel_.SetWritable(false);
    if (write_complete_callback_) {
      eventloop_.Run([connection = shared_from_this()] {
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
    if (output_.ReadableBytes() == 0) {
      PEDRONET_TRACE("::Close()", *this);
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

  channel_.OnRead([this](auto events, auto now) { return handleRead(now); });
  channel_.OnWrite([this](auto events, auto now) { return handleWrite(); });
  channel_.OnClose([this](auto events, auto now) { return Close(); });
  channel_.OnError([this](auto events, auto now) {
    return handleError(channel_.GetError());
  });

  channel_.SetSelector(eventloop.GetSelector());
}
TcpConnection::~TcpConnection() {
  Close();
  PEDRONET_TRACE("{}::~TcpConnection()", *this);
}
void TcpConnection::Send(Buffer &buffer) {
  eventloop_.AssertInsideLoop();

  if (state_ != State::kConnected) {
    return;
  }

  if (state_ == State::kDisconnected) {
    PEDRONET_WARN("give up sending");
    return;
  }

  ssize_t w0 = trySendingDirect(buffer);
  if (w0 < 0) {
    auto err = channel_.GetError();
    if (err.GetCode() != EWOULDBLOCK) {
      handleError(err);
      return;
    }
  }
  output_.EnsureWriteable(buffer.ReadableBytes());
  size_t w1 = output_.Append(&buffer);
  // TODO: high watermark
  if (w1 != 0) {
    channel_.SetWritable(true);
  }
}
ssize_t TcpConnection::trySendingDirect(Buffer &buffer) {
  eventloop_.AssertInsideLoop();
  if (channel_.Writable()) {
    return 0;
  }
  if (output_.ReadableBytes() != 0) {
    return 0;
  }
  return buffer.Retrieve(&channel_.File());
}
void TcpConnection::ForceClose() {
  if (state_ == State::kConnected) {
    state_ = State::kDisconnecting;
  }

  if (state_ == State::kDisconnecting) {
    PEDRONET_TRACE("::ForceClose()", *this);
    channel_.SetWritable(false);
    channel_.SetReadable(false);
    eventloop_.Deregister(&channel_);
  }
}
} // namespace pedronet
