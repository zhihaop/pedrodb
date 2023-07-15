#ifndef PEDRONET_TCP_CLIENT_H
#define PEDRONET_TCP_CLIENT_H

#include "pedronet/callbacks.h"
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

class TcpClient : pedrolib::noncopyable, pedrolib::nonmovable {
 public:
  enum class State {
    kOffline,
    kConnecting,
    kConnected,
    kDisconnecting,
    kDisconnected
  };

 private:
  EventLoopGroupPtr worker_group_;
  InetAddress address_;
  std::atomic<State> state_{State::kOffline};
  TcpConnectionPtr connection_;
  EventLoop* eventloop_{};

  ConnectionCallback connection_callback_;
  CloseCallback close_callback_;
  MessageCallback message_callback_;
  ErrorCallback error_callback_;
  WriteCompleteCallback write_complete_callback_;
  HighWatermarkCallback high_watermark_callback_;

 private:
  void handleConnection(pedronet::Socket conn);
  void retry(Socket socket, Error reason);
  void raiseConnection();

 public:
  explicit TcpClient(InetAddress address) : address_(std::move(address)) {}

  void SetGroup(EventLoopGroupPtr worker_group) {
    worker_group_ = std::move(worker_group);
  }

  void Start();
  void Close();
  void Shutdown();
  void ForceClose();
  void ForceShutdown();

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

  [[nodiscard]] TcpConnectionPtr GetConnection() const noexcept {
    return connection_;
  }
  
  template <class Packable>
  bool Write(Packable&& packable) {
    if (state_ == State::kConnected) {
      connection_->SendPackable(std::forward<Packable>(packable));
      return true;
    }
    return false;
  }

  void OnWriteComplete(WriteCompleteCallback callback) {
    write_complete_callback_ = std::move(callback);
  }

  void OnHighWatermark(HighWatermarkCallback callback) {
    high_watermark_callback_ = std::move(callback);
  }

  [[nodiscard]] State GetState() const noexcept { return state_; }
  auto GetConnection() noexcept { return connection_; }
};
}  // namespace pedronet

#endif  // PEDRONET_TCP_CLIENT_H