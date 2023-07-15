#ifndef PEDROKV_CODEC_CLIENT_CODEC_H
#define PEDROKV_CODEC_CLIENT_CODEC_H

#include <pedrolib/buffer/array_buffer.h>
#include <pedronet/callbacks.h>
#include <memory>
#include "pedrokv/codec/request.h"
#include "pedrokv/codec/response.h"
#include "pedrokv/logger/logger.h"

namespace pedrokv {

class ClientCodecContext;
using ClientMessageCallback = std::function<void(
    const std::shared_ptr<TcpConnection>&, std::queue<Response<>>&)>;

class ClientCodecContext : std::enable_shared_from_this<ClientCodecContext> {
  std::queue<Response<>> response_;
  ClientMessageCallback callback_;
  ArrayBuffer buffer_;

 public:
  explicit ClientCodecContext(ClientMessageCallback callback)
      : callback_(std::move(callback)) {}

  void HandleMessage(const std::shared_ptr<TcpConnection>& conn,
                     ArrayBuffer* buffer) {

    buffer_.Append(buffer);

    while (true) {
      Response response;
      if (!response.UnPack(&buffer_)) {
        break;
      }
      response_.emplace(std::move(response));
    }

    if (!response_.empty()) {
      callback_(conn, response_);
    }
  }
};

class ClientCodec {
  pedronet::ConnectionCallback connect_callback_;
  pedronet::CloseCallback close_callback_;
  ClientMessageCallback message_callback_;

 public:
  void OnConnect(pedronet::ConnectionCallback callback) {
    connect_callback_ = std::move(callback);
  }

  void OnClose(pedronet::CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnMessage(ClientMessageCallback callback) {
    message_callback_ = std::move(callback);
  }

  pedronet::ConnectionCallback GetOnConnect() {
    return [this](const pedronet::TcpConnectionPtr& conn) {
      auto ctx = std::make_shared<ClientCodecContext>(message_callback_);
      conn->SetContext(ctx);
      if (connect_callback_) {
        connect_callback_(conn);
      }
    };
  }

  pedronet::CloseCallback GetOnClose() {
    return [this](const pedronet::TcpConnectionPtr& conn) {
      conn->SetContext(nullptr);
      if (close_callback_) {
        close_callback_(conn);
      }
    };
  }

  pedronet::MessageCallback GetOnMessage() {
    return [](const pedronet::TcpConnectionPtr& conn, ArrayBuffer& buffer,
              Timestamp now) {
      auto ctx = std::any_cast<std::shared_ptr<ClientCodecContext>>(
          conn->GetContext());
      ctx->HandleMessage(conn, &buffer);
    };
  }
};
}  // namespace pedrokv

#endif  // PEDROKV_CODEC_CLIENT_CODEC_H
