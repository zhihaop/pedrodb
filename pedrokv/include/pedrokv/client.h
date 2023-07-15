#ifndef PEDROKV_CLIENT_H
#define PEDROKV_CLIENT_H
#include "pedrokv/codec/client_codec.h"
#include "pedrokv/defines.h"
#include "pedrokv/options.h"

#include <pedrolib/concurrent/latch.h>
#include <pedronet/tcp_client.h>
#include <future>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace pedrokv {

using ResponseCallback = std::function<void(const Response<>&)>;

class Client : nonmovable, noncopyable {
  pedronet::TcpClient client_;
  ClientOptions options_;
  ClientCodec codec_;

  pedronet::ErrorCallback error_callback_;
  pedronet::ConnectionCallback connect_callback_;
  pedronet::CloseCallback close_callback_;

  std::mutex mu_;
  std::condition_variable not_full_;
  std::unordered_map<uint32_t, ResponseCallback> responses_;
  std::atomic_uint32_t request_id_{};

  std::shared_ptr<pedrolib::Latch> close_latch_;

  void HandleResponse(std::queue<Response<>>& responses);

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

  void SendRequest(std::shared_ptr<ArrayBuffer> buffer, uint32_t id,
                   ResponseCallback callback);

  Response<> Get(std::string_view key);

  Response<> Put(std::string_view key, std::string_view value);

  Response<> Delete(std::string_view key);

  void Get(std::string_view key, ResponseCallback callback);

  void Put(std::string_view key, std::string_view value,
           ResponseCallback callback);

  void Delete(std::string_view key, ResponseCallback callback);

  void OnClose(pedronet::CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnError(pedronet::ErrorCallback callback) {
    error_callback_ = std::move(callback);
  }

  void Start();
};
}  // namespace pedrokv

#endif  // PEDROKV_CLIENT_H
