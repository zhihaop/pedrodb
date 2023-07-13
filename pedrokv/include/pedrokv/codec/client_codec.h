#ifndef PEDROKV_CODEC_CLIENT_CODEC_H
#define PEDROKV_CODEC_CLIENT_CODEC_H

#include "pedrokv/codec/request.h"
#include "pedrokv/codec/response.h"
#include "pedrokv/logger/logger.h"
#include <memory>
#include <pedrolib/buffer/array_buffer.h>
#include <pedronet/callbacks.h>

namespace pedrokv {

class ClientCodecContext;
using ClientMessageCallback = std::function<void(
    const std::shared_ptr<TcpConnection> &, std::queue<Response> &)>;

class ClientCodecContext : std::enable_shared_from_this<ClientCodecContext> {
  uint16_t len_{};
  std::queue<Response> response_;
  pedrolib::ArrayBuffer buf_;
  ClientMessageCallback callback_;

public:
  explicit ClientCodecContext(ClientMessageCallback callback)
      : callback_(std::move(callback)) {}

  void HandleMessage(const std::shared_ptr<TcpConnection> &conn,
                     Buffer *buffer) {
    PEDROKV_TRACE("client codec handle bytes: {}", buffer->ReadableBytes());
    while (buffer->ReadableBytes() >= sizeof(len_)) {
      if (len_ == 0) {
        buffer->RetrieveInt(&len_);
        buf_.Reset();
        buf_.EnsureWriteable(len_);
        continue;
      }

      size_t w = std::min(buffer->ReadableBytes(), len_ - buf_.ReadableBytes());
      buf_.Append(buffer->ReadIndex(), w);
      buffer->Retrieve(w);

      if (buf_.ReadableBytes() == len_) {
        response_.emplace().UnPack(&buf_);
        len_ = 0;
      }
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
    return [this](const pedronet::TcpConnectionPtr &conn) {
      auto ctx = std::make_shared<ClientCodecContext>(message_callback_);
      conn->SetContext(ctx);
      if (connect_callback_) {
        connect_callback_(conn);
      }
    };
  }

  pedronet::CloseCallback GetOnClose() {
    return [this](const pedronet::TcpConnectionPtr &conn) {
      conn->SetContext(nullptr);
      if (close_callback_) {
        close_callback_(conn);
      }
    };
  }

  pedronet::MessageCallback GetOnMessage() {
    return [](const pedronet::TcpConnectionPtr &conn, pedronet::Buffer &buffer,
              pedrolib::Timestamp now) {
      auto ctx = std::any_cast<std::shared_ptr<ClientCodecContext>>(
          conn->GetContext());
      ctx->HandleMessage(conn, &buffer);
    };
  }
};
} // namespace pedrokv

#endif // PEDROKV_CODEC_CLIENT_CODEC_H
