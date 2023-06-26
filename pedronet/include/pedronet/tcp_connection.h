#ifndef PEDRONET_TCP_CONNECTION_H
#define PEDRONET_TCP_CONNECTION_H

#include "pedronet/buffer.h"
#include "pedronet/channel/abstract_channel.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/duration.h"
#include "pedronet/core/timestamp.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inet_address.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"

#include "pedronet/core/debug.h"
#include <any>
#include <functional>
#include <memory>
#include <string_view>

#include "pedronet/callbacks.h"

namespace pedronet {
class TcpConnection : public AbstractChannel<TcpConnection> {
public:
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };

protected:
  InetAddress local_;
  InetAddress peer_;
  State state_{TcpConnection::State::kConnecting};
  Socket socket_;

  MessageCallback message_cb_{};
  WriteCompleteCallback write_complete_cb_{};
  HighWatermarkCallback high_watermark_cb_{};
  CloseCallback close_cb_{};
  std::any ctx_{};

  std::unique_ptr<Buffer> output_buffer_ = std::make_unique<ArrayBuffer>();
  std::unique_ptr<Buffer> input_buffer_ = std::make_unique<ArrayBuffer>();

public:
  TcpConnection(Socket socket)
      : AbstractChannel(), local_(socket.GetLocalAddress()),
        peer_(socket.GetPeerAddress()), socket_(std::move(socket)) {}

  ~TcpConnection() override { spdlog::info("connection closed"); }

  core::File &File() noexcept override { return socket_; }
  const core::File &File() const noexcept override { return socket_; }

  void SetContext(const std::any &ctx) { ctx_ = ctx; }
  std::any &GetContext() { return ctx_; }

  void SetMessageCallback(MessageCallback cb) { message_cb_ = std::move(cb); }

  void SetWriteCompleteCallback(WriteCompleteCallback cb) {
    write_complete_cb_ = std::move(cb);
  }

  void SetHighWatermarkCallback(HighWatermarkCallback cb) {
    high_watermark_cb_ = std::move(cb);
  }

  void SetCloseCallback(CloseCallback cb) { close_cb_ = std::move(cb); }

  const InetAddress &GetLocalAddress() const noexcept { return local_; }
  const InetAddress &GetPeerAddress() const noexcept { return peer_; }

  void Attach(EventLoop *loop, const CallBack &cb) override {
    AbstractChannel<TcpConnection>::Attach(loop, cb);
    state_ = State::kConnected;
  }

  void Send(std::string data) {
    if (state_ != State::kConnected) {
      spdlog::trace("ignore sending data[{}]", data);
      return;
    }
    if (loop_->CheckInsideLoop()) {
      HandleSend(std::move(data));
      return;
    }
    loop_->Submit([conn = shared_from_this(), data = std::move(data)] {
      conn->HandleSend(std::move(data));
    });
  }

  ssize_t TrySendingDirect(const std::string &data) {
    if (events_.Contains(SelectEvents::kWriteEvent)) {
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
      EnableEvent(SelectEvents::kWriteEvent);
    }
  }

  void HandleRead(ReceiveEvents events, core::Timestamp now) override {
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

  void HandleClose(ReceiveEvents events, core::Timestamp now) override {
    if (state_ == State::kDisconnected) {
      return;
    }

    state_ = State::kDisconnecting;
    if (state_ == State::kDisconnecting) {
      if (input_buffer_->ReadableBytes() == 0 &&
          output_buffer_->ReadableBytes() == 0) {
        spdlog::trace("HandleClose {}", String());
        state_ = State::kDisconnected;
        SetEvents(SelectEvents::kNoneEvent);
        if (close_cb_) {
          close_cb_(shared_from_this());
        }
      }
    }
  }

  void HandleError(ReceiveEvents events, core::Timestamp now) override {
    auto err = socket_.GetError();
    spdlog::error("TcpConnection error, reason[{}]", err.GetReason());
  }

  void HandleWrite(ReceiveEvents events, core::Timestamp now) override {
    if (!events_.Contains(SelectEvents::kWriteEvent)) {
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
      DisableEvent(SelectEvents::kWriteEvent);
      if (write_complete_cb_) {
        loop_->Submit(std::bind(write_complete_cb_, shared_from_this()));
      }

      if (state_ == State::kDisconnecting) {
        socket_.CloseWrite();
      }
    }
  }

  void Close() { this->Detach({}); }

  std::string String() const override {
    return fmt::format("TcpConnection[local={}, peer={}, fd={}]", local_, peer_,
                       socket_.Descriptor());
  }
};
} // namespace pedronet

PEDRONET_FORMATABLE_CLASS(pedronet::TcpConnection);
#endif // PEDRONET_TCP_CONNECTION_H