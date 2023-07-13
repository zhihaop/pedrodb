#include "pedrokv/client.h"

namespace pedrokv {

void Client::SendRequest(std::shared_ptr<Buffer> buffer, uint32_t id,
                         ResponseCallback callback) {
  std::unique_lock lock{mu_};

  while (responses_.size() > options_.max_inflight) {
    not_full_.wait(lock);
  }

  if (responses_.count(id)) {
    Response response;
    response.id = id;
    response.type = Response::Type::kError;
    callback(response);
    return;
  }

  responses_[id] = std::move(callback);
  lock.unlock();

  if (!client_.Send(std::move(buffer))) {
    Response response;
    response.id = id;
    response.type = Response::Type::kError;

    lock.lock();
    responses_[id](response);
    responses_.erase(id);
  }
}

void Client::Start() {
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
      for (auto &[_, callback] : responses_) {
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
      [this](auto &conn, auto &responses) { HandleResponse(responses); });

  client_.OnError(error_callback_);
  client_.OnMessage(codec_.GetOnMessage());
  client_.OnClose(codec_.GetOnClose());
  client_.OnConnect(codec_.GetOnConnect());
  client_.Start();

  latch->Await();
}

Response Client::Get(std::string_view key) {
  pedrolib::Latch latch(1);
  Response response;
  Get(key, [&](const Response &resp) mutable {
    response = resp;
    latch.CountDown();
  });
  latch.Await();
  return response;
}

Response Client::Put(std::string_view key, std::string_view value) {
  pedrolib::Latch latch(1);
  Response response;
  Put(key, value, [&](const Response &resp) mutable {
    response = resp;
    latch.CountDown();
  });
  latch.Await();
  return response;
}

Response Client::Delete(std::string_view key) {
  pedrolib::Latch latch(1);
  Response response;
  Delete(key, [&](const Response &resp) mutable {
    response = resp;
    latch.CountDown();
  });
  latch.Await();
  return response;
}

void Client::Get(std::string_view key, ResponseCallback callback) {
  auto len = Request::SizeOf(key, {});
  auto buffer = std::make_shared<pedrolib::ArrayBuffer>(len);
  uint32_t id = request_id_.fetch_add(1);
  buffer->AppendInt(len);
  Request::Pack(Request::kGet, id, key, {}, buffer.get());
  return SendRequest(buffer, id, std::move(callback));
}

void Client::Put(std::string_view key, std::string_view value,
                 ResponseCallback callback) {
  auto len = Request::SizeOf(key, value);
  auto buffer = std::make_shared<pedrolib::ArrayBuffer>(len);
  uint32_t id = request_id_.fetch_add(1);
  buffer->AppendInt(len);
  Request::Pack(Request::kSet, id, key, value, buffer.get());
  return SendRequest(buffer, id, std::move(callback));
}

void Client::Delete(std::string_view key, ResponseCallback callback) {
  auto len = Request::SizeOf(key, {});
  auto buffer = std::make_shared<pedrolib::ArrayBuffer>(len);
  uint32_t id = request_id_.fetch_add(1);
  buffer->AppendInt(len);
  Request::Pack(Request::kDelete, id, key, {}, buffer.get());
  return SendRequest(buffer, id, std::move(callback));
}

void Client::HandleResponse(std::queue<Response> &responses) {
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
      callback(response);
    }
    responses_.erase(it);
  }
  not_full_.notify_all();
}
}