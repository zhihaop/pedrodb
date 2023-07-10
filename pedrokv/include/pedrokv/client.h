#ifndef PEDROKV_CLIENT_H
#define PEDROKV_CLIENT_H
#include "pedrokv/codec/client_codec.h"
#include "pedrokv/defines.h"
#include "pedrokv/options.h"

#include <future>
#include <mutex>
#include <pedrolib/concurrent/latch.h>
#include <pedronet/tcp_client.h>
#include <unordered_map>

namespace pedrokv {

class Client : nonmovable, noncopyable {
  pedronet::TcpClient client_;
  ClientOptions options_;
  ClientCodec codec_;
  TcpConnection *conn_{};

  pedronet::ErrorCallback error_callback_;
  pedronet::ConnectionCallback connect_callback_;
  pedronet::CloseCallback close_callback_;

  std::mutex mu_;
  uint16_t request_id_{};
  pedronet::ArrayBuffer buffer_;
  std::unordered_map<uint16_t, std::promise<Response>> responses_;

  void HandleResponse(ClientCodecContext &ctx, const Response &response) {
    std::unique_lock lock{mu_};
    PEDROKV_INFO("handle response id {}", response.id);

    if (!responses_.count(response.id)) {
      return;
    }

    responses_[response.id].set_value(response);
    responses_.erase(response.id);
  }

  void SendRequest(const Request &request) {
    PEDROKV_INFO("send request id: {}, size: {}", request.id,
                 request.SizeOf() + sizeof(uint16_t));
    buffer_.Reset();
    buffer_.AppendInt(request.SizeOf());
    request.Pack(&buffer_);
    conn_->Send(&buffer_);
  }

public:
  Client(InetAddress address, ClientOptions options)
      : client_(std::move(address)), options_(std::move(options)) {
    client_.SetGroup(options_.worker_group);
  }

  ~Client() {
    // TODO fix core dump in ClientCodecContext
  }

  void OnConnect(pedronet::ConnectionCallback callback) {
    connect_callback_ = std::move(callback);
  }

  std::future<Response> Get(std::string_view key) {
    std::unique_lock lock{mu_};
    uint16_t id = request_id_++;

    auto &promise = responses_[id];
    Request request;
    request.type = Request::Type::kGet;
    request.id = id;
    request.key = key;
    SendRequest(request);
    return promise.get_future();
  }

  std::future<Response> Put(std::string_view key, std::string_view value) {
    std::unique_lock lock{mu_};
    uint16_t id = request_id_++;

    auto &promise = responses_[id];
    Request request;
    request.type = Request::Type::kSet;
    request.id = id;
    request.key = key;
    request.value = value;
    SendRequest(request);
    return promise.get_future();
  }

  std::future<Response> Delete(std::string_view key) {
    std::unique_lock lock{mu_};
    uint16_t id = request_id_++;

    auto &promise = responses_[id];
    Request request;
    request.type = Request::Type::kDelete;
    request.id = id;
    request.key = key;
    SendRequest(request);
    return promise.get_future();
  }

  void OnClose(pedronet::CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnError(pedronet::ErrorCallback callback) {
    error_callback_ = std::move(callback);
  }

  void Start() {
    auto latch = std::make_shared<pedrolib::Latch>(1);
    codec_.OnConnect([=](auto &&conn) {
      conn_ = conn.get();
      if (connect_callback_) {
        connect_callback_(conn);
      }
      latch->CountDown();
    });

    codec_.OnClose([=](auto &&conn) {
      conn_ = nullptr;
      if (close_callback_) {
        close_callback_(conn);
      }
      latch->CountDown();
    });

    codec_.OnMessage([this](ClientCodecContext &ctx, const Response &response) {
      HandleResponse(ctx, response);
    });

    client_.OnError(error_callback_);
    client_.OnMessage(codec_.GetOnMessage());
    client_.OnClose(codec_.GetOnClose());
    client_.OnConnect(codec_.GetOnConnect());
    client_.Start();

    latch->Await();
  }
};
} // namespace pedrokv

#endif // PEDROKV_CLIENT_H
