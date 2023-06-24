#ifndef PEDRONET_TCP_CONNECTION_H
#define PEDRONET_TCP_CONNECTION_H

#include "core/duration.h"
#include "core/timestamp.h"

#include "buffer.h"
#include "channel.h"
#include "event_loop.h"
#include "inet_address.h"
#include "selector.h"
#include "socket.h"

#include <any>
#include <fmt/format.h>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>

namespace pedronet {
class TcpConnection : public AbstractChannel<TcpConnection> {
public:
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };
  using Ptr = std::shared_ptr<TcpConnection>;

  using MessageCallback =
      std::function<void(const Ptr &, Buffer *, core::Timestamp)>;
  using WriteCompleteCallback = std::function<void(const Ptr &)>;
  using HighWatermarkCallback = std::function<void(const Ptr &, size_t)>;
  using CloseCallback = std::function<void(const Ptr &)>;
  using ErrorCallback = std::function<void(const Ptr &)>;

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
  TcpConnection(const InetAddress &local, Acception acc)
      : AbstractChannel(), local_(local), peer_(acc.peer),
        socket_(std::move(acc.file)) {}

  static TcpConnection::Ptr Create(const InetAddress &local, Acception acc) {
    return std::make_shared<TcpConnection>(local, std::move(acc));
  }

  ~TcpConnection() override { spdlog::info("connection closed"); }

  core::File &File() noexcept override { return socket_; }

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

  const InetAddress &LocalAddress() const noexcept { return local_; }
  const InetAddress &PeerAddress() const noexcept { return peer_; }

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
    if (events_.Contains(Selector::kWriteEvent)) {
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
      EnableEvent(Selector::kWriteEvent);
    }
  }

  void HandleRead(ReceiveEvent events, core::Timestamp now) override {
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

  void HandleClose(ReceiveEvent events, core::Timestamp now) override {
    if (state_ == State::kDisconnecting) {
      spdlog::trace("HandleClose {}", String());
      state_ = State::kDisconnected;
      SetEvents(Selector::kNoneEvent);
      if (close_cb_) {
        close_cb_(shared_from_this());
      }
    }
  }

  void HandleError(ReceiveEvent events, core::Timestamp now) override {
    auto err = socket_.GetError();
    spdlog::error("TcpConnection error, reason[{}]", err.GetReason());
  }

  void HandleWrite(ReceiveEvent events, core::Timestamp now) override {
    if (!events_.Contains(Selector::kWriteEvent)) {
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
      DisableEvent(Selector::kWriteEvent);
      if (write_complete_cb_) {
        loop_->Submit(std::bind(write_complete_cb_, shared_from_this()));
      }

      if (state_ == State::kDisconnecting) {
        socket_.CloseWrite();
      }
    }
  }

  void Close() { this->Detach({}); }

  std::string String() override {
    return fmt::format("TcpConnection[local={}, peer={}, fd={}]",
                       local_.String(), peer_.String(), socket_.Descriptor());
  }
};
} // namespace pedronet
#endif // PEDRONET_TCP_CONNECTION_H