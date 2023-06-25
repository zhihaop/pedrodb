#ifndef PEDRONET_EVENT_LOOP_H
#define PEDRONET_EVENT_LOOP_H

#include "core/duration.h"
#include "event.h"
#include "selector.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace pedronet {

using CallBack = std::function<void()>;

struct EventLoop : public Selector {
  // For pedronet::Channel.
  virtual void Update(Channel *channel, SelectEvents events,
                      const CallBack &cb) override = 0;
  virtual void Register(const ChannelPtr &channel,
                        const CallBack &cb) override = 0;
  virtual void Deregister(const ChannelPtr &channel,
                          const CallBack &cb) override = 0;

  // For users.
  virtual bool CheckInsideLoop() const noexcept = 0;
  virtual void AssertInsideLoop() const = 0;
  virtual void Loop() = 0;
  virtual void Close() = 0;
  virtual bool Closed() const noexcept = 0;

  virtual void Submit(CallBack cb) = 0;
  virtual uint64_t ScheduleAfter(CallBack cb, core::Duration delay) = 0;
  virtual uint64_t ScheduleEvery(CallBack cb, core::Duration delay,
                                 core::Duration interval) = 0;
  virtual void ScheduleCancel(uint64_t) = 0;

  ~EventLoop() override = default;
};

class EventLoopGroup : core::noncopyable, core::nonmoveable {
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
    for (size_t i = 0; i < loops_.size(); ++i) {
      loops_[i]->Close();
    }
  }
};
} // namespace pedronet

#endif // PEDRONET_EVENT_LOOP_H