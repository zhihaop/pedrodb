#ifndef PRACTICE_PROCESS_POOL_H
#define PRACTICE_PROCESS_POOL_H

#include "controller.h"
#include "ipc.h"
#include "worker.h"
#include <atomic>
#include <future>
#include <memory>

namespace pedro {
class ProcessPool {
  std::vector<std::shared_ptr<Worker>> workers_;
  std::atomic_uint64_t turn_{};

public:
  void AddWorker(std::shared_ptr<Worker> worker) {
    workers_.emplace_back(worker);
  }

  ProcessPool() = default;
  ProcessPool(ProcessPool &&other) : workers_(std::move(other.workers_)) {}

  ~ProcessPool() { Sync(); }

  std::shared_ptr<Worker> RemoveWorker() {
    auto w = std::move(workers_.back());
    workers_.pop_back();
    return w;
  }

  void Submit(pedro::Method method, pedro::Parameter args) {
    uint64_t turn =
        turn_.fetch_add(1, std::memory_order_relaxed) % workers_.size();
    workers_[turn]->Submit(method, args);
  }

  std::future<Any> Call(pedro::Method method, pedro::Parameter args) {
    uint64_t turn =
        turn_.fetch_add(1, std::memory_order_relaxed) % workers_.size();
    return workers_[turn]->Call(method, args);
  }

  const std::vector<std::shared_ptr<Worker>> &Workers() const noexcept {
    return workers_;
  }

  void Sync() const {
    for (const auto &worker : workers_) {
      worker->Sync();
    }
  }
};
} // namespace pedro

#endif // PRACTICE_PROCESS_POOL_H
