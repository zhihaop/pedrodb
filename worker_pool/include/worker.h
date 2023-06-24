#ifndef WORKER_POOL_WORKER_H
#define WORKER_POOL_WORKER_H

#include "channel.h"
#include "controller.h"
#include "ipc.h"

#include <memory>
#include <type_traits>

namespace pedro {

class Worker {
  pid_t process_id_;
  std::unique_ptr<Ipc> ipc_;

public:
  Worker(pid_t pid, std::unique_ptr<Ipc> &&ipc)
      : process_id_(pid), ipc_(std::move(ipc)) {}
  Worker(const Worker &w) = delete;
  Worker &operator=(const Worker &w) = delete;
  Worker &operator=(Worker &&w) noexcept = delete;
  Worker(Worker &&w) noexcept = delete;
  ~Worker();

  static std::shared_ptr<Worker> CreateWorker();
  bool Joinable() const noexcept;

  std::future<Any> Call(pedro::Method method, Parameter args) {
    return ipc_->Call(method, IpcType::REQUEST_RESPONSE, args);
  }

  void Submit(pedro::Method method, Parameter args) {
    ipc_->Call(method, IpcType::FIRE_AND_FORGET, args);
  }

  void Join();
  void Sync();
  void Close();
};

using WorkerPtr = std::shared_ptr<Worker>;

} // namespace pedro

#endif // WORKER_POOL_WORKER_H
