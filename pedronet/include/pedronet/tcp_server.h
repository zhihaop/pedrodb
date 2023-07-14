#ifndef PEDRONET_TCP_SERVER_H
#define PEDRONET_TCP_SERVER_H

#include "pedrolib/buffer/buffer.h"
#include "pedronet/acceptor.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/eventloopgroup.h"
#include "pedronet/inetaddress.h"
#include "pedronet/selector/selector.h"
#include "pedronet/socket.h"
#include "pedronet/tcp_connection.h"

#include <unordered_map>
#include <unordered_set>

namespace pedronet {

class TcpServer : pedrolib::noncopyable, pedrolib::nonmovable {
  std::shared_ptr<EventLoopGroup> boss_group_;
  std::shared_ptr<EventLoopGroup> worker_group_;

  std::shared_ptr<Acceptor> acceptor_;

  ConnectionCallback connection_callback_;
  MessageCallback message_callback_;
  WriteCompleteCallback write_complete_callback_;
  ErrorCallback error_callback_;
  CloseCallback close_callback_;
  HighWatermarkCallback high_watermark_callback_;

  std::mutex mu_;
  std::unordered_set<std::shared_ptr<TcpConnection>> actives_;

 public:
  TcpServer() = default;
  ~TcpServer() { Close(); }

  void SetGroup(const std::shared_ptr<EventLoopGroup>& boss,
                const std::shared_ptr<EventLoopGroup>& worker) {
    boss_group_ = boss;
    worker_group_ = worker;
  }

  void Bind(const InetAddress& address);

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

}  // namespace pedronet

#endif  // PEDRONET_TCP_SERVER_H