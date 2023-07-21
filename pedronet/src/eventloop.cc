#include "pedronet/eventloop.h"
#include "pedronet/logger/logger.h"

namespace pedronet {

void EventLoop::ProcessScheduleTask() {
  std::unique_lock<std::mutex> lock(mu_);
  std::swap(running_tasks_, pending_tasks_);
  lock.unlock();

  while (!running_tasks_.empty()) {
    auto task = std::move(running_tasks_.front());
    running_tasks_.pop();

    task();
  }
}

void EventLoop::Loop() {
  PEDRONET_TRACE("EventLoop::Loop() running");

  auto& current = core::Thread::Current();
  current.BindEventLoop(this);

  SelectChannels selected;
  while (state()) {
    auto err = selector_->Wait(kSelectTimeout, &selected);
    if (!err.Empty()) {
      PEDRONET_ERROR("failed to call selector_.Wait(): {}", err);
      continue;
    }

    size_t n_events = selected.channels.size();
    for (size_t i = 0; i < n_events; ++i) {
      Channel* ch = selected.channels[i];
      ReceiveEvents event = selected.events[i];
      ch->HandleEvents(event, selected.now);
    }
  }

  current.UnbindEventLoop(this);
}

void EventLoop::Close() {
  state_ = 0;

  PEDRONET_TRACE("EventLoop is shutting down.");
  event_channel_.WakeUp();
  // TODO: await shutdown ?
}

void EventLoop::Schedule(Callback cb) {
  PEDRONET_TRACE("submit task");
  std::unique_lock<std::mutex> lock(mu_);
  pending_tasks_.emplace(std::move(cb));
  size_t n = pending_tasks_.size();

  if (n == 1) {
    event_channel_.WakeUp();
  }
}

void EventLoop::AssertUnderLoop() const {
  if (!CheckUnderLoop()) {
    PEDRONET_FATAL("check in event loop failed");
  }
}

void EventLoop::Register(Channel* channel, Callback register_callback,
                         Callback deregister_callback) {
  PEDRONET_INFO("EventLoopImpl::Register({})", *channel);
  if (!CheckUnderLoop()) {
    Schedule([this, channel, r = std::move(register_callback),
              d = std::move(deregister_callback)]() mutable {
      Register(channel, std::move(r), std::move(d));
    });
    return;
  }
  auto it = channels_.find(channel);
  if (it == channels_.end()) {
    selector_->Add(channel, SelectEvents::kNoneEvent);
    channels_.emplace_hint(it, channel, std::move(deregister_callback));
    if (register_callback) {
      register_callback();
    }
  }
}
void EventLoop::Deregister(Channel* channel) {
  if (!CheckUnderLoop()) {
    Schedule([=] { Deregister(channel); });
    return;
  }

  PEDRONET_INFO("EventLoopImpl::Deregister({})", *channel);
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

EventLoop::EventLoop(std::unique_ptr<Selector> selector)
    : selector_(std::move(selector)), timer_queue_(timer_channel_, *this) {
  event_channel_.SetEventCallBack([this]() { ProcessScheduleTask(); });
  selector_->Add(&event_channel_, SelectEvents::kReadEvent);
  selector_->Add(&timer_channel_, SelectEvents::kReadEvent);

  PEDRONET_TRACE("create event loop");
}

void EventLoop::Join() {
  // TODO check joinable.
  close_latch_.Await();
  PEDRONET_INFO("Eventloop join exit");
}

size_t EventLoop::Size() const noexcept {
  return 1;
}

}  // namespace pedronet