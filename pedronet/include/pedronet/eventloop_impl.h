#ifndef PEDRONET_EVENTLOOP_IMPL_H
#define PEDRONET_EVENTLOOP_IMPL_H

#include "pedronet/core/thread.h"

#include "pedronet/channel/channel.h"
#include "pedronet/channel/event_channel.h"
#include "pedronet/event.h"
#include "pedronet/eventloop.h"
#include "pedronet/selector/epoller.h"
#include "pedronet/timer_queue.h"

#include <unordered_set>
#include <vector>

#include "pedronet/core/debug.h"

namespace pedronet {

class EpollEventLoop : public EventLoop {
  inline const static core::Duration kSelectTimeout{std::chrono::seconds(10)};
  inline const static int32_t kMaxEventPerPoll = 10000;

  EpollSelector selector_;
  EventChannel event_ch_;
  TimerChannel timer_ch_;
  TimerQueue timer_queue_;
  Selected selected_ch_;

  std::mutex mu_;
  std::vector<Callback> pending_tasks_;
  std::optional<pid_t> owner_;

  std::atomic_int32_t state_{1};
  std::unordered_map<Channel *, Callback> channels_;

  int32_t state() const noexcept {
    return state_.load(std::memory_order_acquire);
  }

  bool compare_and_set_state(int32_t expected, int32_t state) noexcept {
    return state_.compare_exchange_strong(expected, state);
  }

  void executePendingTask() {
    std::vector<std::function<void()>> tasks;

    std::unique_lock<std::mutex> lock(mu_);
    std::swap(tasks, pending_tasks_);
    lock.unlock();

    for (auto &task : tasks) {
      task();
    }
  }

public:
  EpollEventLoop()
      : selector_(kMaxEventPerPoll), timer_queue_(timer_ch_, *this) {
    selector_.Add(&event_ch_, SelectEvents::kReadEvent);
    selector_.Add(&timer_ch_, SelectEvents::kReadEvent);

    spdlog::trace("create event loop");
  }

  Selector *GetSelector() noexcept override { return &selector_; }

  void Deregister(Channel *channel) override {
    if (!CheckInsideLoop()) {
      Schedule([=] { Deregister(channel); });
      return;
    }

    spdlog::trace("EpollEventLoop::Deregister({})", *channel);
    auto it = channels_.find(channel);
    if (it != channels_.end()) {
      auto callback = std::move(it->second);
      selector_.Remove(channel);
      channels_.erase(it);

      if (callback) {
        callback();
      }
    }
  }

  void Register(Channel *channel, Callback callback) override {
    spdlog::trace("EpollEventLoop::Register({})", *channel);
    if (!CheckInsideLoop()) {
      Schedule([this, channel, cb = std::move(callback)]() mutable {
        Register(channel, std::move(cb));
      });
      return;
    }
    auto [_, success] = channels_.emplace(channel, std::move(callback));
    if (success) {
      selector_.Add(channel, SelectEvents::kNoneEvent);
      auto &cb = channels_[channel];
      if (cb) {
        cb();
      }
    }
  }

  bool CheckInsideLoop() const noexcept override {
    if (!owner_.has_value()) {
      return true;
    }
    return owner_ == core::Thread::GetID();
  }

  void AssertInsideLoop() const override {
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
      event_ch_.WakeUp();
    }
  }

  uint64_t ScheduleAfter(Callback cb, core::Duration delay) override {
    return timer_queue_.ScheduleAfter(std::move(cb), delay);
  }

  uint64_t ScheduleEvery(Callback cb, core::Duration delay,
                         core::Duration interval) override {
    return timer_queue_.ScheduleEvery(std::move(cb), delay, interval);
  }

  void ScheduleCancel(uint64_t id) override { timer_queue_.Cancel(id); }

  bool Closed() const noexcept override { return state() == 0; }

  void Close() override {
    state_ = 0;

    spdlog::trace("EventLoop is shutting down.");
    event_ch_.WakeUp();
    // TODO: await shutdown ?
  }

  void Loop() override {
    owner_ = core::Thread::GetID();

    spdlog::trace("EventLoop::Loop() running");
    while (state()) {
      selector_.Wait(kSelectTimeout, &selected_ch_);

      size_t nevents = selected_ch_.channels.size();
      if (nevents == 0) {
        auto err = selected_ch_.error;
        if (!err.Empty()) {
          spdlog::error("failed to call selector_.Wait(), reason[{}]",
                        err.GetReason());
          continue;
        }
        spdlog::trace("Loop: not thing happened");
      }

      for (size_t i = 0; i < nevents; ++i) {
        Channel *ch = selected_ch_.channels[i];
        ReceiveEvents event = selected_ch_.events[i];
        ch->HandleEvents(event, selected_ch_.now);
      }

      executePendingTask();
    }

    owner_.reset();
  }
};

} // namespace pedronet
#endif // PEDRONET_EVENTLOOP_IMPL_H