#ifndef PEDROKV_CODEC_CODEC_H
#define PEDROKV_CODEC_CODEC_H
#include "pedrokv/codec/request.h"
#include "pedrokv/codec/response.h"
#include "pedrokv/defines.h"
#include "pedrokv/logger/logger.h"
#include <pedrolib/buffer/buffer_view.h>
#include <pedronet/callbacks.h>
#include <pedronet/tcp_connection.h>

#include <utility>

namespace pedrokv {

class ServerCodecContext;
using ServerMessageCallback = std::function<void(
    const std::shared_ptr<TcpConnection> &, const Request &)>;

class ServerCodecContext : std::enable_shared_from_this<ServerCodecContext> {
  uint16_t len_{};
  pedrolib::ArrayBuffer buf_;

  Request request_;
  ServerMessageCallback callback_;

public:
  explicit ServerCodecContext(ServerMessageCallback callback)
      : callback_(std::move(callback)) {}

  void HandleMessage(const std::shared_ptr<TcpConnection> &conn,
                     pedrolib::Buffer *buffer) {
    PEDROKV_TRACE("server codec: Read");
    while (buffer->ReadableBytes() > sizeof(len_)) {
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
        request_.UnPack(&buf_);
        len_ = 0;

        if (callback_) {
          callback_(conn, request_);
        }
      }
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

  static void SendResponse(const std::shared_ptr<TcpConnection> &conn,
                           const Response &response) {
    auto buffer = std::make_shared<pedrolib::ArrayBuffer>(response.SizeOf() +
                                                          sizeof(uint16_t));
    PEDROKV_TRACE("send response bytes: {}",
                  response.SizeOf() + sizeof(uint16_t));
    buffer->AppendInt(response.SizeOf());
    response.Pack(buffer.get());
    conn->Send(buffer);
  }

  pedronet::ConnectionCallback GetOnConnect() {
    return [this](const pedronet::TcpConnectionPtr &conn) {
      auto ctx = std::make_shared<ServerCodecContext>(message_callback_);
      conn->SetContext(ctx);

      if (connect_callback_) {
        connect_callback_(conn);
      }
    };
  }

  pedronet::CloseCallback GetOnClose() {
    return [this](const pedronet::TcpConnectionPtr &conn) {
      if (close_callback_) {
        close_callback_(conn);
      }
    };
  }

  pedronet::MessageCallback GetOnMessage() {
    return [](const pedronet::TcpConnectionPtr &conn, pedronet::Buffer &buffer,
              pedrolib::Timestamp now) {
      auto ctx = std::any_cast<std::shared_ptr<ServerCodecContext>>(
          conn->GetContext());
      ctx->HandleMessage(conn, &buffer);
    };
  }
};
} // namespace pedrokv

#endif // PEDROKV_CODEC_CODEC_H
