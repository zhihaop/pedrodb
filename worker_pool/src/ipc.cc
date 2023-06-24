#include "ipc.h"
#include "proto/rpc.pb.h"
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>

#include "controller.h"

namespace pedro {

std::unique_ptr<Ipc> Ipc::Create() { return std::make_unique<Ipc>(); }

bool Ipc::Valid() const noexcept {
  return request_channel_->Valid() && response_channel_->Valid();
}

bool Ipc::ReadRequest(pb::IpcRequest &request) {
  return request_channel_->Read(request);
}

bool Ipc::SendResponse(const pb::IpcResponse &response) {
  return response_channel_->Write(response, true);
}

pb::IpcRequest GetIpcRequest(Method method, IpcType type, Parameter args) {
  pb::IpcRequest request;
  request.set_arch(Controller::Arch());
  request.set_type(type);
  request.set_handle(static_cast<uint64_t>(reinterpret_cast<size_t>(method)));
  if (args) {
    request.mutable_arguments()->CopyFrom(*args);
  }
  return request;
}

std::future<Any> Ipc::Call(Method method, IpcType type, Parameter args) {
  if (type == IpcType::FIRE_AND_FORGET) {

    auto task = [this, method, args] {
      pb::IpcRequest request =
          GetIpcRequest(method, IpcType::FIRE_AND_FORGET, args);
      request_channel_->Write(request, false);
    };

    if (!args || args->ByteSizeLong() < 64) {
      task();
    } else {
      while (!executor_.Submit(task)) {
        using namespace std::chrono_literals;
        std::this_thread::yield();
      }
    }

    return std::async(std::launch::deferred, [] { return Any{}; });
  }

  if (type == IpcType::REQUEST_RESPONSE) {
    auto task =
        std::make_shared<std::packaged_task<Any()>>([this, method, args] {
          pb::IpcRequest request =
              GetIpcRequest(method, IpcType::REQUEST_RESPONSE, args);
          request_channel_->Write(request, true);

          pb::IpcResponse response;
          response_channel_->Read(response);
          return response.value();
        });

    while (!executor_.Submit([task]() { (*task)(); })) {
      std::this_thread::yield();
    }
    return task->get_future();
  }

  return {};
}

void Ipc::Close() {
  request_channel_->Close();
  response_channel_->Close();
}

} // namespace pedro