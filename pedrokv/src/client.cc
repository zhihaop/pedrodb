#include "pedrokv/client.h"

namespace pedrokv {

void Client::SendRequest(Request<> request, uint32_t id,
                         ResponseCallback callback) {
  std::unique_lock lock{mu_};

  while (responses_.size() > options_.max_inflight) {
    not_full_.wait(lock);
  }

  if (responses_.count(id)) {
    Response response;
    response.id = id;
    response.type = ResponseType::kError;
    callback(response);
    return;
  }
  
  responses_[id] = std::move(callback);

  if (!client_.Write(std::move(request))) {
    Response response;
    response.id = id;
    response.type = ResponseType::kError;
    
    responses_[id](response);
    responses_.erase(id);
  }
}

void Client::Start() {
  auto latch = std::make_shared<pedrolib::Latch>(1);
  codec_.OnConnect([=, &latch](auto&& conn) {
    if (connect_callback_) {
      connect_callback_(conn);
    }
    close_latch_ = std::make_shared<pedrolib::Latch>(1);
    latch->CountDown();
  });

  codec_.OnClose([=](auto&& conn) {
    {
      std::unique_lock lock{mu_};
      Response response;
      response.type = ResponseType::kError;
      for (auto& [_, callback] : responses_) {
        if (callback) {
          callback(response);
        }
      }
      responses_.clear();
      not_full_.notify_all();
    }

    if (close_callback_) {
      close_callback_(conn);
    }
    close_latch_->CountDown();
  });

  codec_.OnMessage(
      [this](auto& conn, auto& responses) { HandleResponse(responses); });

  client_.OnError(error_callback_);
  client_.OnMessage(codec_.GetOnMessage());
  client_.OnClose(codec_.GetOnClose());
  client_.OnConnect(codec_.GetOnConnect());
  client_.Start();

  latch->Await();
}

void Client::Get(std::string_view key, ResponseCallback callback) {
  auto id = request_id_.fetch_add(1);
  Request request;
  request.type = RequestType::kGet;
  request.id = id;
  request.key = key;
  return SendRequest(std::move(request), id, std::move(callback));
}

void Client::Put(std::string_view key, std::string_view value,
                 ResponseCallback callback) {
  auto id = request_id_.fetch_add(1);
  Request request;
  request.type = RequestType::kPut;
  request.id = id;
  request.key = key;
  request.value = value;
  return SendRequest(std::move(request), id, std::move(callback));
}

void Client::Delete(std::string_view key, ResponseCallback callback) {
  auto id = request_id_.fetch_add(1);
  Request request;
  request.type = RequestType::kDelete;
  request.id = id;
  request.key = key;
  return SendRequest(std::move(request), id, std::move(callback));
}

void Client::HandleResponse(std::queue<Response<>>& responses) {
  std::unique_lock lock{mu_};

  while (!responses.empty()) {
    auto response = std::move(responses.front());
    responses.pop();
    auto it = responses_.find(response.id);
    if (it == responses_.end()) {
      return;
    }

    auto callback = std::move(it->second);
    if (callback) {
      callback(std::move(response));
    }
    responses_.erase(it);
  }
  not_full_.notify_all();
}
}  // namespace pedrokv