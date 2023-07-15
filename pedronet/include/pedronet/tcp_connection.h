#ifndef PEDRONET_TCP_CONNECTION_H
#define PEDRONET_TCP_CONNECTION_H

#include "pedrolib/buffer/array_buffer.h"
#include "pedrolib/buffer/buffer.h"
#include "pedronet/callbacks.h"
#include "pedronet/channel/socket_channel.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inetaddress.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"

#include <any>
#include <memory>

namespace pedronet {
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
  std::any ctx_{};

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

  void SetContext(const std::any& ctx) { ctx_ = ctx; }
  std::any& GetContext() { return ctx_; }

  State GetState() const noexcept { return state_; }

  void Start();

  template <class BufferPtr>
  void Send(BufferPtr buffer) {
    if (eventloop_.CheckUnderLoop()) {
      handleSend(buffer.get());
      return;
    }

    eventloop_.Schedule(
        [this, buf = std::move(buffer)]() mutable { handleSend(buf.get()); });
  }

  void handleSend(ArrayBuffer* buffer) {
    std::string_view sv{buffer->ReadIndex(), buffer->ReadableBytes()};
    handleSend(sv);
    buffer->Retrieve(sv.size());
  }

  void Send(ArrayBuffer* buffer) {
    if (eventloop_.CheckUnderLoop()) {
      handleSend(buffer);
      return;
    }

    ArrayBuffer clone(buffer->ReadableBytes());
    clone.Append(buffer);

    eventloop_.Schedule(
        [this, buf = std::move(clone)]() mutable { handleSend(&buf); });
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
  ssize_t trySendingDirect(std::string_view buffer);
  void handleSend(std::string_view buffer);
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::TcpConnection);
#endif  // PEDRONET_TCP_CONNECTION_H