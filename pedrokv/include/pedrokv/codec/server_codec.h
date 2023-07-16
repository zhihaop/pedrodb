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
using ResponseSender = std::function<void(const Response<>&)>;
using ServerMessageCallback = std::function<void(
    const TcpConnectionPtr&, const ResponseSender&, const RequestView&)>;

class ServerCodecContext : public ChannelContext,
                           std::enable_shared_from_this<ServerCodecContext> {
  ArrayBuffer buffer_;
  std::mutex mu_;
  ArrayBuffer output_;
  ServerMessageCallback callback_;

 public:
  explicit ServerCodecContext(ServerMessageCallback callback)
      : callback_(std::move(callback)) {}

  void HandleMessage(const TcpConnectionPtr& conn, ArrayBuffer* buffer) {
    ResponseSender sender([&](const Response<>& response) {
      std::unique_lock lock{mu_};
      response.Pack(&output_);
    });

    while (true) {
      RequestView request;
      if (buffer_.ReadableBytes()) {
        if (request.UnPack(&buffer_)) {
          callback_(conn, sender, request);
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

      if (request.UnPack(buffer)) {
        callback_(conn, sender, request);
        continue;
      }

      break;
    }

    std::unique_lock lock{mu_};
    if (output_.ReadableBytes()) {
      conn->Send(&output_);
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
      auto ctx = conn->GetContext().get();
      ((ServerCodecContext*)ctx)->HandleMessage(conn, &buffer);
    };
  }
};
}  // namespace pedrokv

#endif  // PEDROKV_CODEC_CODEC_H
