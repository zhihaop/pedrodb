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
  ClientMessageCallback callback_;
  ArrayBuffer buffer_;
  std::queue<Response<>> response_;

 public:
  explicit ClientCodecContext(ClientMessageCallback callback)
      : callback_(std::move(callback)) {}

  void HandleMessage(const TcpConnectionPtr& conn, ArrayBuffer* buffer) {
    
    while (true) {
      Response response;
      if (buffer_.ReadableBytes()) {
        if (response.UnPack(&buffer_)) {
          response_.emplace(std::move(response));
          continue;
        }

        uint16_t len;
        if (!PeekInt(&buffer_, &len)) {
          buffer_.Append(buffer);
          continue;
        }

        len = std::min(len - buffer_.ReadableBytes(), buffer->ReadableBytes());
        buffer_.Append(buffer->ReadIndex(), len);
        buffer->Retrieve(len);
        continue;
      }

      if (response.UnPack(buffer)) {
        response_.emplace(std::move(response));
        continue;
      }

      break;
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
