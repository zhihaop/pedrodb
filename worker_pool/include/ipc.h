#ifndef PRACTICE_IPC_H
#define PRACTICE_IPC_H

#include "channel.h"
#include "proto/rpc.pb.h"
#include "thread_pool.h"

#include <atomic>
#include <future>
#include <latch>
#include <memory>

namespace pedro {

using IpcType = pb::IpcType;
using Any = google::protobuf::Any;

using Parameter = std::shared_ptr<Any>;
using Method = std::unique_ptr<Message> (*)(Parameter args);

inline Parameter MakeParameter(Message *message) {
  if (message == nullptr) {
    return {};
  }
  auto any = std::make_shared<Any>();
  any->PackFrom(*message);
  return any;
}

class Ipc {
protected:
  std::unique_ptr<Channel> request_channel_;
  std::unique_ptr<Channel> response_channel_;

  FixedThreadPoolExecutor executor_;

public:
  Ipc()
      : request_channel_(Channel::CreateChannel()),
        response_channel_(Channel::CreateChannel()), executor_(1, 1) {}
  Ipc(const Ipc &rpc) = delete;
  Ipc &operator=(const Ipc &rpc) = delete;
  Ipc(const Ipc &&rpc) = delete;
  Ipc &operator=(Ipc &&rpc) noexcept = delete;

  static std::unique_ptr<Ipc> Create();

  bool Valid() const noexcept;

  void Close();

  bool ReadRequest(pb::IpcRequest &request);
  bool SendResponse(const pb::IpcResponse &response);

  std::future<Any> Call(Method method, IpcType type, Parameter args);
};
} // namespace pedro
#endif // PRACTICE_IPC_H