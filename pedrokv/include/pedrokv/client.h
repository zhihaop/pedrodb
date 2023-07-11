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

  pedronet::ErrorCallback error_callback_;
  pedronet::ConnectionCallback connect_callback_;
  pedronet::CloseCallback close_callback_;

  std::mutex mu_;
  std::condition_variable not_full_;
  uint32_t request_id_{};
  std::unordered_map<uint32_t, std::promise<Response>> responses_;

  std::shared_ptr<pedrolib::Latch> close_latch_;

  void HandleResponse(const Response &response) {
    std::unique_lock lock{mu_};
    PEDROKV_INFO("handle response id {}", response.id);

    auto it = responses_.find(response.id);
    if (it == responses_.end()) {
      return;
    }

    it->second.set_value(response);
    responses_.erase(it);
    not_full_.notify_all();
  }

public:
  Client(InetAddress address, ClientOptions options)
      : client_(std::move(address)), options_(std::move(options)) {
    client_.SetGroup(options_.worker_group);
  }

  ~Client() { Close(); }

  void Close() {
    if (close_latch_ != nullptr) {
      client_.Shutdown();
      close_latch_->Await();
    }
  }

  void OnConnect(pedronet::ConnectionCallback callback) {
    connect_callback_ = std::move(callback);
  }

  std::future<Response> SendRequest(Request &request) {
    std::unique_lock lock{mu_};

    while (responses_.size() > options_.max_inflight) {
      not_full_.wait(lock);
    }

    uint32_t id = request_id_++;

    if (responses_.count(id)) {
      PEDROKV_FATAL("should not happened");
    }

    auto future = [this, id] { return responses_[id].get_future(); }();
    lock.unlock();
    
    auto buffer = std::make_shared<pedrolib::ArrayBuffer>(sizeof(uint16_t) +
                                                          request.SizeOf());

    request.id = id;
    buffer->AppendInt(request.SizeOf());
    request.Pack(buffer.get());

    if (!client_.Send(buffer)) {
      Response response;
      response.id = id;
      response.type = Response::Type::kError;
      HandleResponse(response);
    }
    return future;
  }

  std::future<Response> Get(std::string_view key) {
    Request request;
    request.type = Request::Type::kGet;
    request.key = key;
    return SendRequest(request);
  }

  std::future<Response> Put(std::string_view key, std::string_view value) {
    Request request;
    request.type = Request::Type::kSet;
    request.key = key;
    request.value = value;
    return SendRequest(request);
  }

  std::future<Response> Delete(std::string_view key) {
    Request request;
    request.type = Request::Type::kDelete;
    request.key = key;
    return SendRequest(request);
  }

  void OnClose(pedronet::CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnError(pedronet::ErrorCallback callback) {
    error_callback_ = std::move(callback);
  }

  void Start() {
    auto latch = std::make_shared<pedrolib::Latch>(1);
    codec_.OnConnect([=, &latch](auto &&conn) {
      if (connect_callback_) {
        connect_callback_(conn);
      }
      close_latch_ = std::make_shared<pedrolib::Latch>(1);
      latch->CountDown();
    });

    codec_.OnClose([=](auto &&conn) {
      {
        std::unique_lock lock{mu_};
        Response response;
        response.type = Response::Type::kError;
        for (auto &[k, v] : responses_) {
          v.set_value(response);
        }
        responses_.clear();
        not_full_.notify_all();
      }

      if (close_callback_) {
        close_callback_(conn);
      }
      close_latch_->CountDown();
    });

    codec_.OnMessage([this](auto &conn, const Response &response) {
      HandleResponse(response);
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
