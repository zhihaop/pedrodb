#include "pedronet/timer_queue.h"

namespace pedronet {
void TimerQueue::updateExpire(core::Timestamp) {
  if (schedule_timer_.empty() || next_expire_ <= schedule_timer_.top().expire) {
    return;
  }
  channel_.WakeUpAt(schedule_timer_.top().expire);
}
void TimerQueue::selectExpiredTimer(core::Timestamp now) {
  spdlog::trace("select timers, size[{}]", schedule_timer_.size());
  while (!schedule_timer_.empty() && schedule_timer_.top().expire <= now) {
    auto timer = schedule_timer_.top().timer;
    schedule_timer_.pop();
    expired_timers_.emplace(std::move(timer));
  }
}
void TimerQueue::processExpireTimer() {
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
void TimerQueue::processPendingTimer(core::Timestamp now) {
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

    if (timer->interval > core::Duration::Zero()) {
      schedule_timer_.emplace(now + timer->interval, std::move(weak_timer));
    } else {
      timers_.erase(timer->id);
    }
  }
  updateExpire(now);
}

TimerQueue::TimerQueue(TimerChannel &channel, core::Executor &executor)
    : channel_(channel), executor_(executor) {
  channel.SetEventCallBack([this](ReceiveEvents event, core::Timestamp now) {
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

uint64_t TimerQueue::createTimer(Callback cb, const core::Duration &delay,
                                 const core::Duration &interval) {
  core::Timestamp now = core::Timestamp::Now();
  uint64_t id = ++sequences_;
  spdlog::trace("create timer {}", id);

  auto timer = std::make_shared<TimerStruct>(id, std::move(cb), interval);
  schedule_timer_.emplace(now + delay, timer);
  timers_.emplace(id, std::move(timer));

  executor_.Schedule([this, now] { updateExpire(now); });
  return id;
}
uint64_t TimerQueue::ScheduleEvery(Callback callback,
                                   const core::Duration &delay,
                                   const core::Duration &interval) {
  std::unique_lock<std::mutex> lock(mu_);
  return createTimer(std::move(callback), delay, interval);
}
uint64_t TimerQueue::ScheduleAfter(Callback callback,
                                   const core::Duration &delay) {
  std::unique_lock<std::mutex> lock(mu_);
  return createTimer(std::move(callback), delay, core::Duration::Seconds(0));
}
void TimerQueue::Cancel(uint64_t timer_id) {
  std::unique_lock<std::mutex> lock(mu_);
  timers_.erase(timer_id);
}
} // namespace pedronet