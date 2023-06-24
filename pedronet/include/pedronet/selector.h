#ifndef PEDRONET_SELECTOR_H
#define PEDRONET_SELECTOR_H

#include "core/noncopyable.h"
#include "core/nonmoveable.h"
#include "core/timestamp.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace pedronet {

struct Channel;
using CallBack = std::function<void()>;
using ChannelPtr = std::shared_ptr<Channel>;

class SelectorEvents {
  uint32_t events_;

public:
  explicit SelectorEvents(uint32_t events) : events_(events) {}

  uint32_t Value() const noexcept { return events_; }

  bool Contains(SelectorEvents other) const noexcept {
    return events_ & other.events_;
  }

  SelectorEvents &Add(SelectorEvents other) noexcept {
    events_ |= other.events_;
    return *this;
  }
  SelectorEvents &Remove(SelectorEvents other) noexcept {
    events_ &= ~other.events_;
    return *this;
  }
};

using ReceiveEvent = uint32_t;
using SelectorCallback = std::function<void(ReceiveEvent, core::Timestamp)>;

struct Selector : core::noncopyable, core::nonmoveable {
  static const SelectorEvents kNoneEvent;
  static const SelectorEvents kReadEvent;
  static const SelectorEvents kWriteEvent;

  // For pedronet::Channel.
  virtual void Update(Channel *channel, SelectorEvents events,
                      const CallBack &cb) = 0;
  virtual void Register(const ChannelPtr &channel, const CallBack &cb) = 0;
  virtual void Deregister(const ChannelPtr &channel, const CallBack &cb) = 0;
  virtual ~Selector() = default;
};
} // namespace pedronet
#endif // PEDRONET_SELECTOR_H