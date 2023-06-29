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

  std::unique_ptr<Buffer> output_ = std::make_unique<ArrayBuffer>();
  std::unique_ptr<Buffer> input_ = std::make_unique<ArrayBuffer>();

  SocketChannel channel_;
  InetAddress local_;
  InetAddress peer_;
  EventLoop &eventloop_;

public:
  TcpConnection(EventLoop &eventloop, Socket socket);

  ~TcpConnection();

  void SetContext(const std::any &ctx) { ctx_ = ctx; }
  std::any &GetContext() { return ctx_; }

  State GetState() const noexcept { return state_; }

  void Start();

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

  void Send(const std::string &data);

  ssize_t TrySendingDirect(const std::string &data);

  void HandleRead(ReceiveEvents events, core::Timestamp now);

  void HandleError(ReceiveEvents events, core::Timestamp now);

  void WriteBuffer(ReceiveEvents events, core::Timestamp now);

  void Close();

  std::string String() const;
};
} // namespace pedronet

PEDRONET_CLASS_FORMATTER(pedronet::TcpConnection)
#endif // PEDRONET_TCP_CONNECTION_H