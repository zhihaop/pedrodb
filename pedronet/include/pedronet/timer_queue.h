#ifndef PEDRONET_TIMER_QUEUE
#define PEDRONET_TIMER_QUEUE

#include "pedronet/selector/selector.h"
#include "pedronet/channel/timer_channel.h"

#include <queue>

namespace pedronet {
struct TimerStruct : core::noncopyable, core::nonmoveable {
  uint64_t id;
  std::function<void()> callback;
  core::Duration interval;

  template <class Callback>
  TimerStruct(uint64_t id, Callback &&cb, core::Duration interval)
      : id(id), callback(std::forward<Callback>(cb)), interval(interval) {}
};

struct TimerOrder {
  core::Timestamp expire;
  std::weak_ptr<TimerStruct> timer;

  TimerOrder(core::Timestamp expire, const std::weak_ptr<TimerStruct> &timer)
      : expire(expire), timer(timer) {}

  bool operator<(const TimerOrder &other) const noexcept {
    return expire > other.expire;
  }
};

class TimerQueue {

  TimerChannel &timer_ch_;

  core::Timestamp next_expire_ = core::Timestamp::Max();
  std::priority_queue<TimerOrder> schedule_timer_;
  std::queue<std::weak_ptr<TimerStruct>> expired_timers_;
  std::queue<std::weak_ptr<TimerStruct>> pending_timers_;

  std::mutex mu_;
  uint64_t sequences_{};
  std::unordered_map<uint64_t, std::shared_ptr<TimerStruct>> timers_;

  void updateExpire(core::Timestamp now) {
    if (schedule_timer_.empty() ||
        next_expire_ <= schedule_timer_.top().expire) {
      return;
    }
    timer_ch_.WakeUpAt(schedule_timer_.top().expire);
  }

  template <typename Callback>
  uint64_t createTimer(Callback &&cb, core::Duration delay,
                       core::Duration interval) {
    core::Timestamp now = core::Timestamp::Now();
    uint64_t id = ++sequences_;
    spdlog::trace("create timer {}", id);

    auto timer =
        std::make_shared<TimerStruct>(id, std::forward<Callback>(cb), interval);
    schedule_timer_.emplace(now + delay, timer);
    timers_.emplace(id, std::move(timer));

    updateExpire(now);
    return id;
  }

  void selectExpiredTimer(core::Timestamp now) {
    spdlog::trace("select timers, size[{}]", schedule_timer_.size());
    while (!schedule_timer_.empty() && schedule_timer_.top().expire <= now) {
      auto timer = std::move(schedule_timer_.top().timer);
      schedule_timer_.pop();
      expired_timers_.emplace(std::move(timer));
    }
  }

  void processExpireTimer() {
    spdlog::trace("invoke expire timers[{}]", expired_timers_.size());

    while (!expired_timers_.empty()) {
      auto weak_timer = std::move(expired_timers_.front());
      expired_timers_.pop();

      if (weak_timer.expired()) {
        continue;
      }

      auto timer = weak_timer.lock();
      if (timer == nullptr) {
        continue;
      }

      timer->callback();

      pending_timers_.emplace(std::move(weak_timer));
    }
  }

  void processPendingTimer(core::Timestamp now) {
    while (!pending_timers_.empty()) {
      auto weak_timer = std::move(pending_timers_.front());
      pending_timers_.pop();
      if (weak_timer.expired()) {
        continue;
      }

      auto timer = weak_timer.lock();
      if (timer == nullptr) {
        continue;
      }

      if (timer->interval > 0) {
        schedule_timer_.emplace(now + timer->interval, std::move(weak_timer));
      } else {
        timers_.erase(timer->id);
      }
    }
    updateExpire(now);
  }

public:
  TimerQueue(TimerChannel &timer_ch) : timer_ch_(timer_ch) {
    timer_ch.SetEventCallBack(
        [this](ReceiveEvents event, core::Timestamp now) {
          spdlog::trace("invoke timer ch");
          std::unique_lock<std::mutex> lock(mu_);
          next_expire_ = core::Timestamp::Max();

          selectExpiredTimer(core::Timestamp::Now());
          lock.unlock();
          processExpireTimer();
          lock.lock();
          processPendingTimer(core::Timestamp::Now());
          lock.unlock();
        });
  }

  ~TimerQueue() { timer_ch_.SetEventCallBack({}); }

  template <typename Action>
  uint64_t ScheduleAfter(Action &&callback, core::Duration delay) {
    std::unique_lock<std::mutex> lock(mu_);
    return createTimer(std::forward<Action>(callback), delay,
                       core::Duration::Seconds(0));
  }

  template <typename Callback>
  uint64_t ScheduleEvery(Callback &&cb, core::Duration delay,
                         core::Duration interval) {
    std::unique_lock<std::mutex> lock(mu_);
    return createTimer(std::forward<Callback>(cb), delay, interval);
  }

  void Cancal(uint64_t timer_id) {
    std::unique_lock<std::mutex> lock(mu_);
    timers_.erase(timer_id);
  }
};
} // namespace pedronet
#endif // PEDRONET_TIMER_QUEUE