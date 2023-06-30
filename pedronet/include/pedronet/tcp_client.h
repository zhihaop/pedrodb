#ifndef PEDRONET_TCP_CLIENT_H
#define PEDRONET_TCP_CLIENT_H

#include "pedronet/callbacks.h"
#include "pedronet/core/debug.h"
#include "pedronet/core/latch.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/eventloopgroup.h"
#include "pedronet/inetaddress.h"
#include "pedronet/socket.h"
#include "pedronet/tcp_connection.h"

#include <memory>
#include <unordered_set>
#include <utility>

namespace pedronet {

class TcpClient : core::noncopyable, core::nonmovable {
  std::shared_ptr<EventLoopGroup> worker_group_;
  InetAddress address_;

  std::mutex mu_;
  std::unordered_set<std::shared_ptr<TcpConnection>> actives_;

  ConnectionCallback connection_callback_;
  CloseCallback close_callback_;
  MessageCallback message_callback_;
  ErrorCallback error_callback_;
  WriteCompleteCallback write_complete_callback_;
  HighWatermarkCallback high_watermark_callback_;

private:
  void connecting(EventLoop &conn, Socket socket);

  void retry(EventLoop &loop, Socket socket, core::File::Error reason);

  void connect(EventLoop &loop);

public:
  explicit TcpClient(InetAddress address) : address_(std::move(address)) {}

  void SetGroup(std::shared_ptr<EventLoopGroup> worker_group) {
    worker_group_ = std::move(worker_group);
  }

  void Start();
  void Close();

  void OnConnect(ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
  }

  void OnClose(CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnMessage(MessageCallback callback) {
    message_callback_ = std::move(callback);
  }

  void OnError(ErrorCallback callback) {
    error_callback_ = std::move(callback);
  }

  void OnWriteComplete(WriteCompleteCallback callback) {
    write_complete_callback_ = std::move(callback);
  }

  void OnHighWatermark(HighWatermarkCallback callback) {
    high_watermark_callback_ = std::move(callback);
  }
};
} // namespace pedronet

#endif // PEDRONET_TCP_CLIENT_H