#ifndef PEDRONET_EVENT_H
#define PEDRONET_EVENT_H

#include <cstdint>
#include <initializer_list>

namespace pedronet {

class SelectEvents {
  uint32_t events_{};

public:
  static const SelectEvents kNoneEvent;
  static const SelectEvents kReadEvent;
  static const SelectEvents kWriteEvent;

  SelectEvents() = default;

  explicit SelectEvents(uint32_t events) : events_(events) {}

  uint32_t Value() const noexcept { return events_; }

  bool Contains(SelectEvents other) const noexcept {
    return events_ & other.events_;
  }

  SelectEvents &Add(SelectEvents other) noexcept {
    events_ |= other.events_;
    return *this;
  }
  SelectEvents &Remove(SelectEvents other) noexcept {
    events_ &= ~other.events_;
    return *this;
  }
};

class ReceiveEvents {
  uint32_t events_{};

public:
  static const ReceiveEvents kHangUp;
  static const ReceiveEvents kInvalid;
  static const ReceiveEvents kError;
  static const ReceiveEvents kReadable;
  static const ReceiveEvents kPriorReadable;
  static const ReceiveEvents kReadHangUp;
  static const ReceiveEvents kWritable;

  ReceiveEvents() = default;

  explicit ReceiveEvents(uint32_t events) : events_(events) {}

  uint32_t Value() const noexcept { return events_; }

  bool Contains(ReceiveEvents other) const noexcept {
    return events_ & other.events_;
  }

  bool OneOf(const std::initializer_list<ReceiveEvents>& events) {
    for (auto e: events) {
      if (Contains(e)) {
        return true;
      }
    }
    return false;
  }

  ReceiveEvents &Add(ReceiveEvents other) noexcept {
    events_ |= other.events_;
    return *this;
  }
  ReceiveEvents &Remove(ReceiveEvents other) noexcept {
    events_ &= ~other.events_;
    return *this;
  }
};
} // namespace pedronet
#endif // PEDRONET_EVENT_H