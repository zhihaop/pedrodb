#ifndef PEDRONET_CHANNEL_H
#define PEDRONET_CHANNEL_H

#include "core/file.h"
#include "event.h"
#include "event_loop.h"

#include <fmt/format.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace pedronet {
struct Channel : core::noncopyable, core::nonmoveable {

  // For pedronet::Selector.
  virtual core::File &File() noexcept = 0;
  virtual void HandleEvents(ReceiveEvents events, core::Timestamp now) = 0;
  virtual std::string String() {
    return fmt::format("Channel[fd={}]", File().Descriptor());
  }

  virtual ~Channel() = default;
};

using ChannelPtr = std::shared_ptr<Channel>;

template <class ChannelImpl>
class AbstractChannel : public Channel,
                        public std::enable_shared_from_this<ChannelImpl> {
protected:
  SelectEvents events_{SelectEvents::kNoneEvent};
  EventLoop *loop_{};

public:
  template <class... Args>
  AbstractChannel() : Channel(), std::enable_shared_from_this<ChannelImpl>() {}

  ~AbstractChannel() override = default;

  virtual void Attach(EventLoop *loop, const CallBack &cb) {
    if (loop_ == nullptr) {
      loop_ = loop;
      loop_->Register(this->shared_from_this(), cb);
    }
  }

  virtual void Detach(const CallBack &cb) {
    if (loop_ != nullptr) {
      loop_->Deregister(this->shared_from_this(), cb);
      loop_ = nullptr;
    }
  }

  // For pedronet::Selector.
  void HandleEvents(ReceiveEvents events, core::Timestamp now) override final {
    spdlog::info("{} handel events[{}]", String(), events.Value());
    if (events.Contains(ReceiveEvents::kHangUp) &&
        !events.Contains(ReceiveEvents::kReadable)) {
      HandleClose(events, now);
    }

    if (events.OneOf({ReceiveEvents::kError, ReceiveEvents::kInvalid})) {
      HandleError(events, now);
    }

    if (events.OneOf({ReceiveEvents::kReadable, ReceiveEvents::kPriorReadable,
                      ReceiveEvents::kReadHangUp})) {
      HandleRead(events, now);
    }

    if (events.Contains(ReceiveEvents::kWritable)) {
      HandleWrite(events, now);
    }
  }

  // For users.
  virtual void HandleRead(ReceiveEvents events, core::Timestamp now) {
    spdlog::trace("ignore read event on channel[{}]",
                  reinterpret_cast<uintptr_t>(this));
  }

  virtual void HandleError(ReceiveEvents events, core::Timestamp now) {
    spdlog::warn("ignore error event on channel[{}]",
                 reinterpret_cast<uintptr_t>(this));
  }

  virtual void HandleWrite(ReceiveEvents events, core::Timestamp now) {
    spdlog::trace("ignore write event on channel[{}]",
                  reinterpret_cast<uintptr_t>(this));
  }

  virtual void HandleClose(ReceiveEvents events, core::Timestamp now) {
    spdlog::trace("ignore close event on channel[{}]",
                  reinterpret_cast<uintptr_t>(this));
  }

  void DisableEvent(SelectEvents event) {
    events_.Remove(event);
    UpdateEvent({});
  }

  void EnableEvent(SelectEvents event) {
    events_.Add(event);
    UpdateEvent({});
  }

  SelectEvents Events() const noexcept { return events_; }

  void SetEvents(SelectEvents events) {
    events_ = events;
    UpdateEvent({});
  }

  void UpdateEvent(const CallBack &cb) {
    spdlog::info("{} update events[{}]", String(), events_.Value());
    loop_->Update(this, events_, cb);
  }

  core::File &File() noexcept override = 0;
};

} // namespace pedronet

#endif // PEDRONET_CHANNEL_H