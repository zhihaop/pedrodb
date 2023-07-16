#ifndef PEDRONET_TCP_CONNECTION_H
#define PEDRONET_TCP_CONNECTION_H

#include "pedrolib/buffer/array_buffer.h"
#include "pedrolib/buffer/buffer.h"
#include "pedronet/callbacks.h"
#include "pedronet/channel/socket_channel.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inetaddress.h"
#include "pedronet/logger/logger.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"

#include <any>
#include <memory>

namespace pedronet {

class TcpConnection;

struct ChannelContext {};

class TcpConnection : pedrolib::noncopyable,
                      pedrolib::nonmovable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };

 protected:
  std::atomic<State> state_{TcpConnection::State::kConnecting};

  MessageCallback message_callback_{};
  WriteCompleteCallback write_complete_callback_{};
  HighWatermarkCallback high_watermark_callback_{};
  ErrorCallback error_callback_;
  CloseCallback close_callback_;
  ConnectionCallback connection_callback_{};
  std::shared_ptr<ChannelContext> ctx_;

  ArrayBuffer output_;
  ArrayBuffer input_;

  SocketChannel channel_;
  InetAddress local_;
  InetAddress peer_;
  EventLoop& eventloop_;

  void handleRead(Timestamp now);
  void handleError(Error);
  void handleWrite();

  void handleClose();

 public:
  TcpConnection(EventLoop& eventloop, Socket socket);

  ~TcpConnection();

  void SetContext(const std::shared_ptr<ChannelContext>& ctx) { ctx_ = ctx; }
  const auto& GetContext() const noexcept { return ctx_; }

  State GetState() const noexcept { return state_; }

  void Start();

  template <class Packable>
  void SendPackable(Packable&& packable) {
    if (eventloop_.CheckUnderLoop()) {
      if (GetState() != State::kConnected) {
        return;
      }
      packable.Pack(&output_);

      if (output_.ReadableBytes()) {
        channel_.SetWritable(true);
      }
      handleWrite();
      return;
    }

    eventloop_.Schedule(
        [this, clone = std::forward<Packable>(packable)]() mutable {
          SendPackable(std::forward<Packable>(clone));
        });
  }

  void Send(ArrayBuffer* buf) {
    if (eventloop_.CheckUnderLoop()) {
      std::string_view view{buf->ReadIndex(), buf->ReadableBytes()};
      handleSend(view);
      buf->Reset();
      return;
    }

    PEDRONET_ERROR("schedule!");
    std::string clone(buf->ReadIndex(), buf->ReadableBytes());
    buf->Reset();
    eventloop_.Schedule([this, clone = std::move(clone)]() { Send(clone); });
  }

  void Send(std::string_view buffer) {
    if (eventloop_.CheckUnderLoop()) {
      handleSend(buffer);
      return;
    }

    eventloop_.Run([this, clone = std::string(buffer)] { handleSend(clone); });
  }

  void Send(std::string buffer) {
    eventloop_.Run([this, clone = std::move(buffer)] { handleSend(clone); });
  }

  void OnMessage(MessageCallback cb) { message_callback_ = std::move(cb); }

  void OnError(ErrorCallback cb) { error_callback_ = std::move(cb); }

  void OnWriteComplete(WriteCompleteCallback cb) {
    write_complete_callback_ = std::move(cb);
  }

  void OnHighWatermark(HighWatermarkCallback cb) {
    high_watermark_callback_ = std::move(cb);
  }

  void OnConnection(ConnectionCallback cb) {
    connection_callback_ = std::move(cb);
  }

  void OnClose(CloseCallback cb) { close_callback_ = std::move(cb); }

  const InetAddress& GetLocalAddress() const noexcept { return local_; }
  const InetAddress& GetPeerAddress() const noexcept { return peer_; }

  void Close();
  void Shutdown();
  void ForceShutdown();
  void ForceClose();

  EventLoop& GetEventLoop() noexcept { return eventloop_; }

  std::string String() const;
  void handleSend(std::string_view buffer);
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::TcpConnection);
#endif  // PEDRONET_TCP_CONNECTION_H