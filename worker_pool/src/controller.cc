#include "controller.h"
#include "proto/rpc.pb.h"

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace pedro::Controller {

static std::atomic_bool shutdown_;

pb::MachineArch Arch() {
  if constexpr (sizeof(void *) == 8) {
    return pb::MachineArch::x64;
  }
  return pb::MachineArch::x86;
}

std::unique_ptr<Message> Shutdown(Parameter) {
  shutdown_.store(true, std::memory_order_release);
  return {};
}

std::unique_ptr<Message> Sync(Parameter) { return {}; }

Method GetMethod(const pb::IpcRequest &request) {
  if (request.arch() == Arch()) {
    return reinterpret_cast<Method>(request.handle());
  }
  return nullptr;
}

pb::IpcResponse RequestHandler(const pb::IpcRequest &request) {
  pedro::Method method = GetMethod(request);

  if (!method) {
    pb::IpcResponse failure;
    failure.set_success(false);
    return failure;
  }

  pb::IpcResponse success;
  success.set_success(true);

  Parameter args;
  if (request.has_arguments()) {
    args = std::make_unique<Any>(request.arguments());
  }
  auto value = method(std::move(args));
  if (value) {
    success.mutable_value()->PackFrom(*value);
  }
  return success;
}

void Listen(Ipc &ipc) {
  while (!shutdown_.load(std::memory_order_acquire)) {
    pb::IpcRequest request;
    if (!ipc.ReadRequest(request)) {
      std::terminate();
    }

    pb::IpcResponse response = RequestHandler(request);
    if (request.type() == IpcType::REQUEST_RESPONSE) {
      ipc.SendResponse(response);
    }
  }
  exit(0);
}

} // namespace pedro::Controller