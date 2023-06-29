#ifndef PEDRONET_EVENTLOOP_H
#define PEDRONET_EVENTLOOP_H

#include "pedronet/core/duration.h"
#include "pedronet/core/executor.h"
#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"
#include "pedronet/event.h"
#include "pedronet/selector/selector.h"
#include <any>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"

namespace pedronet {

struct EventLoop : public core::Executor {
  // For pedronet::Channel.
  virtual void Update(Channel *channel, SelectEvents events) = 0;
  virtual void Register(Channel *channel, Callback callback) = 0;
  virtual void Deregister(Channel *channel) = 0;

  // For users.
  virtual bool CheckInsideLoop() const noexcept = 0;
  virtual void AssertInsideLoop() const = 0;
  virtual void Loop() = 0;
  virtual void Close() = 0;
  virtual bool Closed() const noexcept = 0;

  template <typename Callback> void Submit(Callback &&callback) {
    if (CheckInsideLoop()) {
      callback();
      return;
    }
    Schedule(std::forward<Callback>(callback));
  }

  void Schedule(Callback cb) override = 0;
  uint64_t ScheduleAfter(Callback cb, core::Duration delay) override = 0;
  uint64_t ScheduleEvery(Callback cb, core::Duration delay,
                         core::Duration interval) override = 0;
  void ScheduleCancel(uint64_t) override = 0;
  virtual ~EventLoop() = default;
};

class EventLoopGroup : core::noncopyable, core::nonmovable {
  const std::vector<std::unique_ptr<EventLoop>> loops_;
  std::vector<std::thread> threads_;
  std::atomic_size_t next_{};

  size_t next() {
    for (;;) {
      size_t c = next_.load(std::memory_order_acquire);
      size_t n = (c + 1) % loops_.size();
      if (!next_.compare_exchange_strong(c, n)) {
        continue;
      }
      return c;
    }
  }

public:
  explicit EventLoopGroup(std::vector<std::unique_ptr<EventLoop>> &&loops_)
      : loops_(std::move(loops_)) {}

  template <typename EventLoopImpl> static auto Create(size_t threads) {
    std::vector<std::unique_ptr<EventLoop>> loops(threads);
    for (size_t i = 0; i < threads; ++i) {
      loops[i] = std::make_unique<EventLoopImpl>();
    }
    return std::make_shared<EventLoopGroup>(std::move(loops));
  }

  ~EventLoopGroup() { Join(); }

  EventLoop &Next() { return *loops_[next()]; }

  void Join() {
    for (auto &t : threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
    threads_.clear();
  }

  void Start() {
    threads_.reserve(loops_.size());
    for (size_t i = 0; i < loops_.size(); ++i) {
      threads_.emplace_back([&loop = loops_[i]] { loop->Loop(); });
    }
  }

  void Close() {
    for (const auto & loop : loops_) {
      loop->Close();
    }
  }
};
} // namespace pedronet

#endif // PEDRONET_EVENTLOOP_H