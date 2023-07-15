#ifndef PEDROKV_CODEC_CODEC_H
#define PEDROKV_CODEC_CODEC_H
#include <pedronet/callbacks.h>
#include <pedronet/tcp_connection.h>
#include "pedrokv/codec/request.h"
#include "pedrokv/codec/response.h"
#include "pedrokv/defines.h"
#include "pedrokv/logger/logger.h"

#include <utility>

namespace pedrokv {

class ServerCodecContext;
using ServerMessageCallback = std::function<void(
    const std::shared_ptr<TcpConnection>&, std::queue<Request<>>&)>;

class ServerCodecContext : std::enable_shared_from_this<ServerCodecContext> {
  pedrolib::ArrayBuffer buffer_;
  std::queue<Request<>> request_;
  ServerMessageCallback callback_;

 public:
  explicit ServerCodecContext(ServerMessageCallback callback)
      : callback_(std::move(callback)) {}

  void HandleMessage(const std::shared_ptr<TcpConnection>& conn,
                     ArrayBuffer* buffer) {

    buffer_.Append(buffer);

    while (true) {
      Request request;
      if (!request.UnPack(&buffer_)) {
        break;
      }
      request_.emplace(std::move(request));
    }

    if (!request_.empty()) {
      callback_(conn, request_);
    }
  }
};

class ServerCodec {
  pedronet::ConnectionCallback connect_callback_;
  pedronet::CloseCallback close_callback_;
  ServerMessageCallback message_callback_;

 public:
  void OnConnect(pedronet::ConnectionCallback callback) {
    connect_callback_ = std::move(callback);
  }

  void OnClose(pedronet::CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnMessage(ServerMessageCallback callback) {
    message_callback_ = std::move(callback);
  }

  pedronet::ConnectionCallback GetOnConnect() {
    return [this](const pedronet::TcpConnectionPtr& conn) {
      auto ctx = std::make_shared<ServerCodecContext>(message_callback_);
      conn->SetContext(ctx);

      if (connect_callback_) {
        connect_callback_(conn);
      }
    };
  }

  pedronet::CloseCallback GetOnClose() {
    return [this](const pedronet::TcpConnectionPtr& conn) {
      if (close_callback_) {
        close_callback_(conn);
      }
    };
  }

  pedronet::MessageCallback GetOnMessage() {
    return [](const pedronet::TcpConnectionPtr& conn, ArrayBuffer& buffer,
              Timestamp now) {
      auto ctx = std::any_cast<std::shared_ptr<ServerCodecContext>>(
          conn->GetContext());
      ctx->HandleMessage(conn, &buffer);
    };
  }
};
}  // namespace pedrokv

#endif  // PEDROKV_CODEC_CODEC_H
