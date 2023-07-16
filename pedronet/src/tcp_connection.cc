#include "pedronet/tcp_connection.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

void TcpConnection::Start() {
  auto conn = shared_from_this();

  auto handle_register = [this, conn] {
    State s = State::kConnecting;
    if (!state_.compare_exchange_strong(s, State::kConnected)) {
      PEDRONET_ERROR("{} has been register to channel", *this);
      return;
    }

    PEDRONET_INFO("handleConnection {}", *this);
    if (connection_callback_) {
      connection_callback_(conn);
    }
    channel_.SetReadable(true);
  };

  auto handle_deregister = [this, conn] {
    if (state_ != State::kDisconnected) {
      PEDRONET_ERROR("should not happened");
      return;
    }

    if (close_callback_) {
      close_callback_(conn);
    }
  };

  eventloop_.Register(&channel_, std::move(handle_register),
                      std::move(handle_deregister));
}

void TcpConnection::handleRead(Timestamp now) {
  ssize_t n = input_.Append(&channel_.GetFile());
  if (n < 0) {
    auto err = Error{errno};
    if (err.GetCode() != EWOULDBLOCK && err.GetCode() != EAGAIN) {
      handleError(err);
    }
    return;
  }

  if (n == 0) {
    PEDRONET_INFO("close because no data");
    handleClose();
    return;
  }

  if (message_callback_) {
    message_callback_(shared_from_this(), input_, now);
  }
}

void TcpConnection::handleError(Error err) {
  if (err.Empty()) {
    PEDRONET_ERROR("unknown error, force close");
    handleClose();
    return;
  }

  if (error_callback_) {
    error_callback_(shared_from_this(), err);
    return;
  }
  
  PEDRONET_ERROR("{}::handleError(): [{}]", *this, err);
  if (err.GetCode() == EPIPE) {
    ForceClose();
    return;
  }
}

void TcpConnection::handleWrite() {
  if (!channel_.Writable()) {
    PEDRONET_TRACE("{} is down, no more writing", *this);
    return;
  }

  if (output_.ReadableBytes()) {
    ssize_t n = channel_.Write(output_.ReadIndex(), output_.ReadableBytes());
    if (n < 0) {
      handleError(channel_.GetError());
      return;
    }
    output_.Retrieve(n);
  }

  if (output_.ReadableBytes() == 0) {
    channel_.SetWritable(false);
    if (write_complete_callback_) {
      write_complete_callback_(shared_from_this());
    }

    if (state_ == State::kDisconnecting) {
      channel_.CloseWrite();
    }
  }
}

void TcpConnection::Close() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnected)) {
    return;
  }

  eventloop_.Run([this] {
    if (output_.ReadableBytes() == 0) {
      PEDRONET_TRACE("{}::Close()", *this);
      channel_.SetWritable(false);
      channel_.SetReadable(false);
      eventloop_.Deregister(&channel_);
    }
  });
}

std::string TcpConnection::String() const {
  return fmt::format("TcpConnection[local={}, peer={}, channel={}]", local_,
                     peer_, channel_);
}

TcpConnection::TcpConnection(EventLoop& eventloop, Socket socket)
    : channel_(std::move(socket)),
      local_(channel_.GetLocalAddress()),
      peer_(channel_.GetPeerAddress()),
      eventloop_(eventloop) {

  channel_.OnRead([this](auto events, auto now) { handleRead(now); });
  channel_.OnWrite([this](auto events, auto now) { handleWrite(); });
  channel_.OnClose([this](auto events, auto now) { handleClose(); });
  channel_.OnError([this](auto, auto) { handleError(channel_.GetError()); });
  channel_.SetSelector(eventloop.GetSelector());
}

TcpConnection::~TcpConnection() {
  PEDRONET_TRACE("{}::~TcpConnection()", *this);
}

void TcpConnection::handleSend(std::string_view buffer) {
  State s = state_;
  if (s != State::kConnected) {
    PEDRONET_WARN("{}::SendPackable(): give up sending buffer", *this);
    return;
  }

  if (!output_.ReadableBytes() && !buffer.empty()) {
    ssize_t w = channel_.Write(buffer.data(), buffer.size());
    if (w < 0) {
      auto err = channel_.GetError();
      if (err.GetCode() != EWOULDBLOCK && err.GetCode() != EAGAIN) {
        handleError(err);
      }
    }
    buffer = buffer.substr(w < 0 ? 0 : w);
  }

  if (!buffer.empty()) {
    size_t w = output_.WritableBytes();
    if (w < buffer.size()) {
      if (high_watermark_callback_) {
        high_watermark_callback_(shared_from_this(), buffer.size() - w);
      }
      output_.EnsureWritable(buffer.size());
    }
    output_.Append(buffer.data(), buffer.size());
    channel_.SetWritable(true);
  }
}

void TcpConnection::ForceClose() {
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnected;
  eventloop_.Run([this] {
    PEDRONET_TRACE("{}::Close()", *this);
    channel_.SetWritable(false);
    channel_.SetReadable(false);
    eventloop_.Deregister(&channel_);
  });
}

void TcpConnection::Shutdown() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  eventloop_.Run([this] {
    if (output_.ReadableBytes() == 0) {
      PEDRONET_TRACE("{}::Close()", *this);
      channel_.SetWritable(false);
      channel_.CloseWrite();
    }
  });
}

void TcpConnection::ForceShutdown() {
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnecting;

  eventloop_.Run([this] {
    PEDRONET_TRACE("{}::Close()", *this);
    channel_.SetWritable(false);
    channel_.CloseWrite();
  });
}

void TcpConnection::handleClose() {
  if (state_ == State::kDisconnected) {
    return;
  }
  PEDRONET_INFO("{}::handleClose()", *this);
  state_ = State::kDisconnected;
  eventloop_.Deregister(&channel_);
}

}  // namespace pedronet
