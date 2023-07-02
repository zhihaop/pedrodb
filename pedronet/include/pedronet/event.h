#ifndef PEDRONET_EVENT_H
#define PEDRONET_EVENT_H

#include <initializer_list>
#include <pedrolib/format/formatter.h>
#include <string>

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
  bool operator!=(const SelectEvents &other) const noexcept {
    return events_ != other.events_;
  }

  std::string String() const noexcept {
    char buf[3]{};
    buf[0] = Contains(kReadEvent) ? 'r' : '-';
    buf[1] = Contains(kWriteEvent) ? 'w' : '-';
    return buf;
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

  bool OneOf(const std::initializer_list<ReceiveEvents> &events) {
    for (auto e : events) {
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

PEDROLIB_CLASS_FORMATTER(pedronet::SelectEvents);
#endif // PEDRONET_EVENT_H