#ifndef PEDRONET_TCP_CONNECTION_H
#define PEDRONET_TCP_CONNECTION_H

#include "callbacks.h"
#include "channel/socket_channel.h"
#include "core/noncopyable.h"
#include "core/nonmovable.h"
#include "event.h"
#include "eventloop.h"
#include "pedronet/buffer.h"
#include "pedronet/channel/socket_channel.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/duration.h"
#include "pedronet/core/timestamp.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inetaddress.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"

#include "pedronet/core/debug.h"
#include <any>
#include <functional>
#include <memory>
#include <string_view>

#include "pedronet/callbacks.h"

namespace pedronet {
class TcpConnection : core::noncopyable,
                      core::nonmovable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };

protected:
  State state_{TcpConnection::State::kConnecting};

  MessageCallback message_cb_{};
  WriteCompleteCallback write_complete_cb_{};
  HighWatermarkCallback high_watermark_cb_{};
  ConnectionCallback connection_cb_{};
  std::any ctx_{};

  std::unique_ptr<Buffer> output_buffer_ = std::make_unique<ArrayBuffer>();
  std::unique_ptr<Buffer> input_buffer_ = std::make_unique<ArrayBuffer>();

  Socket socket_;
  InetAddress local_;
  InetAddress peer_;
  SocketChannel channel_;
  EventLoop &eventloop_;

public:
  TcpConnection(EventLoop &eventloop, Socket socket)
      : socket_(std::move(socket)), local_(socket_.GetLocalAddress()),
        peer_(socket_.GetPeerAddress()), channel_(socket_),
        eventloop_(eventloop) {

    channel_.OnRead(
        [this](auto events, auto now) { return HandleRead(events, now); });
    channel_.OnWrite(
        [this](auto events, auto now) { return HandleWrite(events, now); });
    channel_.OnClose(
        [this](auto events, auto now) { return HandleClose(events, now); });
    channel_.OnError(
        [this](auto events, auto now) { return HandleError(events, now); });

    channel_.OnEventUpdate(
        [this](auto events) { eventloop_.Update(&channel_, events); });
  }

  ~TcpConnection() { spdlog::info("connection closed"); }

  void SetContext(const std::any &ctx) { ctx_ = ctx; }
  std::any &GetContext() { return ctx_; }

  void Start() {
    eventloop_.Register(&channel_, [conn = shared_from_this()] {
      if (conn->state_ == State::kDisconnecting) {
        conn->state_ = State::kDisconnected;
      }
      if (conn->state_ == State::kConnecting) {
        conn->state_ = State::kConnected;
        conn->channel_.SetReadable(true);
      }
      if (conn->connection_cb_) {
        conn->connection_cb_(conn);
      }
    });
  }

  void OnMessage(MessageCallback cb) { message_cb_ = std::move(cb); }

  void OnWriteComplete(WriteCompleteCallback cb) {
    write_complete_cb_ = std::move(cb);
  }

  void OnHighWatermark(HighWatermarkCallback cb) {
    high_watermark_cb_ = std::move(cb);
  }

  void OnConnection(ConnectionCallback cb) { connection_cb_ = std::move(cb); }

  const InetAddress &GetLocalAddress() const noexcept { return local_; }
  const InetAddress &GetPeerAddress() const noexcept { return peer_; }

  void Send(std::string data) {
    if (state_ != State::kConnected) {
      spdlog::trace("ignore sending data[{}]", data);
      return;
    }

    if (eventloop_.CheckInsideLoop()) {
      HandleSend(std::move(data));
      return;
    }
    eventloop_.Submit(
        [conn = shared_from_this(), data = std::move(data)]() mutable {
          conn->HandleSend(std::move(data));
        });
  }

  ssize_t TrySendingDirect(const std::string &data) {
    if (channel_.Writable()) {
      return 0;
    }

    if (output_buffer_->ReadableBytes() != 0) {
      return 0;
    }

    return socket_.Write(data.data(), data.size());
  }

  void HandleSend(std::string data) {
    if (state_ == State::kDisconnected) {
      spdlog::warn("give up sending");
      return;
    }

    ssize_t w0 = TrySendingDirect(data);
    if (w0 < 0) {
      auto err = socket_.GetError();
      if (err.GetCode() != EWOULDBLOCK) {
        spdlog::error("{}::HandleSend throws {}", err.GetReason());
        return;
      }
    }

    output_buffer_->EnsureWriteable(data.size() - w0);
    ssize_t w1 = output_buffer_->Append(data.data() + w0, data.size() - w0);
    // TODO: high watermark
    if (w1 != 0) {
      channel_.SetWritable(true);
    }
  }

  void HandleRead(ReceiveEvents events, core::Timestamp now) {
    ssize_t n = input_buffer_->Append(&socket_, input_buffer_->WritableBytes());
    spdlog::trace("read {} bytes", n);
    if (n < 0) {
      HandleError(events, now);
      return;
    }

    if (n == 0) {
      HandleClose(events, now);
      return;
    }

    if (message_cb_) {
      message_cb_(shared_from_this(), input_buffer_.get(), now);
    }
  }

  void HandleClose(ReceiveEvents events, core::Timestamp now) {
    if (state_ == State::kDisconnected) {
      return;
    }

    state_ = State::kDisconnecting;
    if (state_ == State::kDisconnecting) {
      if (input_buffer_->ReadableBytes() == 0 &&
          output_buffer_->ReadableBytes() == 0) {
        spdlog::trace("HandleClose {}", String());
        state_ = State::kDisconnected;
        channel_.SetWritable(false);
        channel_.SetReadable(false);
        if (connection_cb_) {
          connection_cb_(shared_from_this());
        }
      }
    }
  }

  void HandleError(ReceiveEvents events, core::Timestamp now) {
    auto err = socket_.GetError();
    spdlog::error("TcpConnection error, reason[{}]", err.GetReason());
  }

  void HandleWrite(ReceiveEvents events, core::Timestamp now) {
    if (!channel_.Writable()) {
      spdlog::trace("TcpConnection fd[{}] is down, no more writing",
                    socket_.Descriptor());
      return;
    }

    ssize_t n =
        output_buffer_->Retrieve(&socket_, output_buffer_->ReadableBytes());
    if (n < 0) {
      auto err = socket_.GetError();
      spdlog::error("failed to write socket, reason[{}]", err.GetReason());
      return;
    }
    if (output_buffer_->ReadableBytes() == 0) {
      channel_.SetWritable(false);
      if (write_complete_cb_) {
        eventloop_.Submit([connection = shared_from_this()] {
          connection->write_complete_cb_(connection);
        });
      }

      if (state_ == State::kDisconnecting) {
        socket_.CloseWrite();
      }
    }
  }

  void Close() {
    channel_.Close();
    eventloop_.Deregister(&channel_);
  }

  std::string String() const {
    return fmt::format("TcpConnection[local={}, peer={}, fd={}]", local_, peer_,
                       socket_.Descriptor());
  }
};
} // namespace pedronet

PEDRONET_FORMATABLE_CLASS(pedronet::TcpConnection)
#endif // PEDRONET_TCP_CONNECTION_H