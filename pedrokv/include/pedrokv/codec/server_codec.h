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
using ServerMessageCallback =
    std::function<void(ServerCodecContext &, const Request &)>;

class ServerCodecContext {
  pedronet::TcpConnectionPtr conn_;
  uint16_t len_{};
  pedrolib::ArrayBuffer buf_;

  Request request_;
  ServerMessageCallback callback_;

public:
  explicit ServerCodecContext(pedronet::TcpConnectionPtr conn,
                              ServerMessageCallback callback)
      : conn_(std::move(conn)), callback_(std::move(callback)) {}

  pedronet::TcpConnection *GetConnection() { return conn_.get(); }

  void SetResponse(const Response &response) {
    pedronet::ArrayBuffer buffer;
    PEDROKV_INFO("send response bytes: {}", response.SizeOf() + sizeof(uint16_t));
    buffer.AppendInt(response.SizeOf());
    response.Pack(&buffer);
    conn_->Send(&buffer);
  }

  void HandleMessage(pedrolib::Buffer *buffer) {
    PEDROKV_INFO("server codec: Read");
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
          callback_(*this, request_);
        }
      }
    }
  }
};

class ServerCodec {
  pedronet::ConnectionCallback connect_callback_;
  pedronet::CloseCallback close_callback_;
  pedronet::ErrorCallback error_callback_;
  pedronet::HighWatermarkCallback high_watermark_callback_;
  ServerMessageCallback message_callback_;

public:
  void OnConnect(pedronet::ConnectionCallback callback) {
    connect_callback_ = std::move(callback);
  }

  void OnClose(pedronet::CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnError(pedronet::ErrorCallback callback) {
    error_callback_ = std::move(callback);
  }

  void OnHighWatermark(pedronet::HighWatermarkCallback callback) {
    high_watermark_callback_ = std::move(callback);
  }

  void OnMessage(ServerMessageCallback callback) {
    message_callback_ = std::move(callback);
  }

  pedronet::ConnectionCallback GetOnConnect() {
    return [this](const pedronet::TcpConnectionPtr &conn) {
      ServerCodecContext ctx(conn, message_callback_);
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

  static pedronet::MessageCallback GetOnMessage() {
    return [](const pedronet::TcpConnectionPtr &conn, pedronet::Buffer &buffer,
              pedrolib::Timestamp now) {
      auto &ctx = std::any_cast<ServerCodecContext &>(conn->GetContext());
      ctx.HandleMessage(&buffer);
    };
  }

  pedronet::ErrorCallback GetOnError() { return error_callback_; }

  pedronet::HighWatermarkCallback GetOnHighWatermark() {
    return high_watermark_callback_;
  }
};
} // namespace pedrokv

#endif // PEDROKV_CODEC_CODEC_H
