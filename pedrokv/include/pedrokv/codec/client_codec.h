#ifndef PEDROKV_CODEC_CLIENT_CODEC_H
#define PEDROKV_CODEC_CLIENT_CODEC_H

#include "pedrokv/codec/request.h"
#include "pedrokv/codec/response.h"
#include "pedrokv/logger/logger.h"
#include <pedrolib/buffer/array_buffer.h>
#include <pedronet/callbacks.h>

namespace pedrokv {

class ClientCodecContext;
using ClientMessageCallback =
    std::function<void(ClientCodecContext &, const Response &)>;

class ClientCodecContext {
  pedronet::TcpConnectionPtr conn_;
  uint16_t len_{};
  Response response_;
  pedrolib::ArrayBuffer buf_;
  pedronet::ArrayBuffer output_buf_;
  ClientMessageCallback callback_;

public:
  explicit ClientCodecContext(pedronet::TcpConnectionPtr conn,
                              ClientMessageCallback callback)
      : conn_(std::move(conn)), callback_(std::move(callback)) {}

  void HandleMessage(Buffer *buffer) {
    PEDROKV_INFO("client codec handle bytes: {}", buffer->ReadableBytes());
    while (buffer->ReadableBytes() >= sizeof(len_)) {
      if (len_ == 0) {
        buffer->RetrieveInt(&len_);
        buf_.EnsureWriteable(len_);
        continue;
      }

      size_t w = std::min(buffer->ReadableBytes(), len_ - buf_.ReadableBytes());
      buf_.Append(buffer->ReadIndex(), w);
      buffer->Retrieve(w);

      if (buf_.ReadableBytes() == len_) {
        response_.UnPack(&buf_);
        len_ = 0;

        if (callback_) {
          callback_(*this, response_);
        }
      }
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
      ClientCodecContext ctx(conn, message_callback_);
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
      auto &ctx = std::any_cast<ClientCodecContext &>(conn->GetContext());
      ctx.HandleMessage(&buffer);
    };
  }
};
} // namespace pedrokv

#endif // PEDROKV_CODEC_CLIENT_CODEC_H
