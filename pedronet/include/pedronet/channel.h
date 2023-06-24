#ifndef PEDRONET_CHANNEL_H
#define PEDRONET_CHANNEL_H

#include "core/file.h"
#include "event_loop.h"

#include <memory>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace pedronet {
using ReceiveEvent = uint32_t;

struct Channel : core::noncopyable, core::nonmoveable {
  const static ReceiveEvent kHangUp;
  const static ReceiveEvent kInvalid;
  const static ReceiveEvent kError;
  const static ReceiveEvent kReadable;
  const static ReceiveEvent kPriorReadable;
  const static ReceiveEvent kReadHangUp;
  const static ReceiveEvent kWritable;

  // For pedronet::Selector.
  virtual core::File &File() noexcept = 0;
  virtual void HandleEvents(ReceiveEvent events, core::Timestamp now) = 0;
  virtual std::string String() {
    return fmt::format("Channel[fd={}]", File().Descriptor());
  }

  virtual ~Channel() = default;
};

using ChannelPtr = std::shared_ptr<Channel>;
using ReceiveEvent = uint32_t;

template <class ChannelImpl>
class AbstractChannel : public Channel,
                        public std::enable_shared_from_this<ChannelImpl> {
protected:
  SelectorEvents events_{Selector::kNoneEvent};
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
  void HandleEvents(ReceiveEvent events, core::Timestamp now) override final {
    spdlog::info("{} handel events[{}]", String(), events);
    if ((events & kHangUp) && !(events & kReadable)) {
      HandleClose(events, now);
    }

    if (events & (kError | kInvalid)) {
      HandleError(events, now);
    }

    if (events & (kReadable | kPriorReadable | kReadHangUp)) {
      HandleRead(events, now);
    }

    if (events & kWritable) {
      HandleWrite(events, now);
    }
  }

  // For users.
  virtual void HandleRead(ReceiveEvent events, core::Timestamp now) {
    spdlog::trace("ignore read event on channel[{}]",
                  reinterpret_cast<uintptr_t>(this));
  }

  virtual void HandleError(ReceiveEvent events, core::Timestamp now) {
    spdlog::warn("ignore error event on channel[{}]",
                 reinterpret_cast<uintptr_t>(this));
  }

  virtual void HandleWrite(ReceiveEvent events, core::Timestamp now) {
    spdlog::trace("ignore write event on channel[{}]",
                  reinterpret_cast<uintptr_t>(this));
  }

  virtual void HandleClose(ReceiveEvent events, core::Timestamp now) {
    spdlog::trace("ignore close event on channel[{}]",
                  reinterpret_cast<uintptr_t>(this));
  }

  void DisableEvent(SelectorEvents event) {
    events_.Remove(event);
    UpdateEvent({});
  }

  void EnableEvent(SelectorEvents event) {
    events_.Add(event);
    UpdateEvent({});
  }

  SelectorEvents Events() const noexcept { return events_; }

  void SetEvents(SelectorEvents events) {
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