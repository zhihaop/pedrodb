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
#include <optional>
#include <thread>
#include <vector>

#include "pedronet/callbacks.h"
#include "pedronet/channel/channel.h"
#include "pedronet/channel/event_channel.h"
#include "pedronet/channel/timer_channel.h"
#include "pedronet/core/thread.h"
#include "timer_queue.h"

namespace pedronet {

class EventLoop : public core::Executor {
  inline const static core::Duration kSelectTimeout{std::chrono::seconds(10)};

  std::unique_ptr<Selector> selector_;
  EventChannel event_channel_;
  TimerChannel timer_channel_;
  TimerQueue timer_queue_;

  std::mutex mu_;
  std::vector<Callback> pending_tasks_;
  std::vector<Callback> running_tasks_;
  std::optional<pid_t> owner_;

  std::atomic_int32_t state_{1};
  std::unordered_map<Channel *, Callback> channels_;

  int32_t state() const noexcept {
    return state_.load(std::memory_order_acquire);
  }

public:
  explicit EventLoop(std::unique_ptr<Selector> selector)
      : selector_(std::move(selector)), timer_queue_(timer_channel_, *this) {
    selector_->Add(&event_channel_, SelectEvents::kReadEvent);
    selector_->Add(&timer_channel_, SelectEvents::kReadEvent);

    spdlog::trace("create event loop");
  }

  Selector *GetSelector() noexcept { return selector_.get(); }

  void Deregister(Channel *channel) {
    if (!CheckInsideLoop()) {
      Schedule([=] { Deregister(channel); });
      return;
    }

    spdlog::trace("EventLoopImpl::Deregister({})", *channel);
    auto it = channels_.find(channel);
    if (it == channels_.end()) {
      return;
    }

    auto callback = std::move(it->second);
    selector_->Remove(channel);
    channels_.erase(it);

    if (callback) {
      callback();
    }
  }

  void Register(Channel *channel, Callback callback) {
    spdlog::trace("EventLoopImpl::Register({})", *channel);
    if (!CheckInsideLoop()) {
      Schedule([this, channel, cb = std::move(callback)]() mutable {
        Register(channel, std::move(cb));
      });
      return;
    }
    auto it = channels_.find(channel);
    if (it == channels_.end()) {
      selector_->Add(channel, SelectEvents::kNoneEvent);
      if (callback) {
        callback();
      }
      channels_.emplace_hint(it, channel, std::move(callback));
    }
  }

  bool CheckInsideLoop() const noexcept {
    return owner_ == core::Thread::GetID();
  }

  void AssertInsideLoop() const {
    if (!CheckInsideLoop()) {
      spdlog::error("check in event loop failed, own={}, thd={}",
                    owner_.value_or(-1), core::Thread::GetID());
      std::terminate();
    }
  }

  void Schedule(Callback cb) override {
    spdlog::trace("submit task");
    std::unique_lock<std::mutex> lock(mu_);

    bool wake_up = pending_tasks_.empty();
    pending_tasks_.emplace_back(std::move(cb));
    if (wake_up) {
      event_channel_.WakeUp();
    }
  }

  uint64_t ScheduleAfter(core::Duration delay, Callback cb) override {
    return timer_queue_.ScheduleAfter(delay, std::move(cb));
  }

  uint64_t ScheduleEvery(core::Duration delay, core::Duration interval,
                         Callback cb) override {
    return timer_queue_.ScheduleEvery(delay, interval, std::move(cb));
  }

  void ScheduleCancel(uint64_t id) override { timer_queue_.Cancel(id); }

  template <typename Runnable> void Run(Runnable &&runnable) {
    if (CheckInsideLoop()) {
      runnable();
      return;
    }
    Schedule(std::forward<Runnable>(runnable));
  }

  bool Closed() const noexcept { return state() == 0; }

  void Close() {
    state_ = 0;

    spdlog::trace("EventLoop is shutting down.");
    event_channel_.WakeUp();
    // TODO: await shutdown ?
  }

  void Loop() {
    spdlog::trace("EventLoop::Loop() running");
    owner_ = core::Thread::GetID();
    
    SelectChannels selected;
    while (state()) {
      auto err = selector_->Wait(kSelectTimeout, &selected);
      if (!err.Empty()) {
        spdlog::error("failed to call selector_.Wait(): {}", err);
        continue;
      }

      size_t n_events = selected.channels.size();
      for (size_t i = 0; i < n_events; ++i) {
        Channel *ch = selected.channels[i];
        ReceiveEvents event = selected.events[i];
        ch->HandleEvents(event, selected.now);
      }

      std::unique_lock<std::mutex> lock(mu_);
      std::swap(running_tasks_, pending_tasks_);
      lock.unlock();
      
      for (auto &task : running_tasks_) {
        task();
      }

      running_tasks_.clear();
    }
    owner_.reset();
  }
};

} // namespace pedronet

#endif // PEDRONET_EVENTLOOP_H