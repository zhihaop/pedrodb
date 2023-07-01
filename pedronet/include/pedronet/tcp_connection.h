#ifndef PEDRONET_TCP_CONNECTION_H
#define PEDRONET_TCP_CONNECTION_H

#include "pedronet/buffer.h"
#include "pedronet/callbacks.h"
#include "pedronet/channel/socket_channel.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/duration.h"
#include "pedronet/core/timestamp.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/inetaddress.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"

#include <any>
#include <memory>

namespace pedronet {
class TcpConnection : core::noncopyable,
                      core::nonmovable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };

protected:
  State state_{TcpConnection::State::kConnecting};

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
  EventLoop &eventloop_;

  ssize_t trySendingDirect(Buffer &buffer);
  void handleRead(core::Timestamp now);
  void handleError(Socket::Error);
  void handleWrite();

public:
  TcpConnection(EventLoop &eventloop, Socket socket);

  ~TcpConnection();

  void SetContext(const std::any &ctx) { ctx_ = ctx; }
  std::any &GetContext() { return ctx_; }

  State GetState() const noexcept { return state_; }

  void Start();

  void Send(Buffer &buffer);

  void Send(Buffer &&buffer) { Send(buffer); }

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

  const InetAddress &GetLocalAddress() const noexcept { return local_; }
  const InetAddress &GetPeerAddress() const noexcept { return peer_; }

  void Close();

  void ForceClose();

  EventLoop &GetEventLoop() noexcept { return eventloop_; }

  std::string String() const;
};
} // namespace pedronet

PEDRONET_CLASS_FORMATTER(pedronet::TcpConnection);
#endif // PEDRONET_TCP_CONNECTION_H