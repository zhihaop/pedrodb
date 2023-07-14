#include "pedronet/timer_queue.h"
#include "pedronet/logger/logger.h"

namespace pedronet {
void TimerQueue::updateExpire(Timestamp) {
  if (schedule_timer_.empty() || next_expire_ <= schedule_timer_.top().expire) {
    return;
  }
  channel_.WakeUpAt(schedule_timer_.top().expire);
}
void TimerQueue::selectExpiredTimer(Timestamp now) {
  PEDRONET_TRACE("select timers, size[{}]", schedule_timer_.size());
  while (!schedule_timer_.empty() && schedule_timer_.top().expire <= now) {
    auto timer = schedule_timer_.top().timer;
    schedule_timer_.pop();
    expired_timers_.emplace(std::move(timer));
  }
}
void TimerQueue::processExpireTimer() {
  PEDRONET_TRACE("invoke expire timers[{}]", expired_timers_.size());

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
void TimerQueue::processPendingTimer(Timestamp now) {
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

    if (timer->interval > Duration::Zero()) {
      schedule_timer_.emplace(now + timer->interval, std::move(weak_timer));
    } else {
      timers_.erase(timer->id);
    }
  }
  updateExpire(now);
}

TimerQueue::TimerQueue(TimerChannel& channel, Executor& executor)
    : channel_(channel), executor_(executor) {
  channel.SetEventCallBack([this](ReceiveEvents event, Timestamp now) {
    PEDRONET_TRACE("invoke timer ch");
    std::unique_lock<std::mutex> lock(mu_);
    next_expire_ = Timestamp::Max();

    selectExpiredTimer(Timestamp::Now());
    lock.unlock();
    processExpireTimer();
    lock.lock();
    processPendingTimer(Timestamp::Now());
    lock.unlock();
  });
}

uint64_t TimerQueue::createTimer(Callback cb, const Duration& delay,
                                 const Duration& interval) {
  Timestamp now = Timestamp::Now();
  uint64_t id = ++sequences_;
  PEDRONET_TRACE("create timer {}", id);

  auto timer = std::make_shared<TimerStruct>(id, std::move(cb), interval);
  schedule_timer_.emplace(now + delay, timer);
  timers_.emplace(id, std::move(timer));

  executor_.Schedule([this, now] { updateExpire(now); });
  return id;
}
uint64_t TimerQueue::ScheduleEvery(const Duration& delay,
                                   const Duration& interval,
                                   Callback callback) {
  std::unique_lock<std::mutex> lock(mu_);
  return createTimer(std::move(callback), delay, interval);
}
uint64_t TimerQueue::ScheduleAfter(const Duration& delay, Callback callback) {
  std::unique_lock<std::mutex> lock(mu_);
  return createTimer(std::move(callback), delay, Duration::Seconds(0));
}
void TimerQueue::Cancel(uint64_t timer_id) {
  std::unique_lock<std::mutex> lock(mu_);
  timers_.erase(timer_id);
}
}  // namespace pedronet