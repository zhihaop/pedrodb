#ifndef PEDRONET_EVENT_H
#define PEDRONET_EVENT_H

#include <pedrolib/format/formatter.h>
#include <algorithm>
#include <initializer_list>
#include <string>

namespace pedronet {

enum class SelectTrigger {
  kLevel,
  kEdge,
};

class SelectEvents {
  uint32_t events_{};

 public:
  static const SelectEvents kNoneEvent;
  static const SelectEvents kReadEvent;
  static const SelectEvents kWriteEvent;

  SelectEvents() = default;

  explicit SelectEvents(uint32_t events) : events_(events) {}

  [[nodiscard]] uint32_t Value() const noexcept { return events_; }

  [[nodiscard]] bool Contains(SelectEvents other) const noexcept {
    return events_ & other.events_;
  }

  [[nodiscard]] SelectEvents Trigger(SelectTrigger trigger) const noexcept;

  SelectEvents& Add(SelectEvents other) noexcept {
    events_ |= other.events_;
    return *this;
  }

  SelectEvents& Remove(SelectEvents other) noexcept {
    events_ &= ~other.events_;
    return *this;
  }
  bool operator!=(const SelectEvents& other) const noexcept {
    return events_ != other.events_;
  }

  [[nodiscard]] std::string String() const noexcept {
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

  [[nodiscard]] uint32_t Value() const noexcept { return events_; }

  [[nodiscard]] bool Contains(ReceiveEvents other) const noexcept {
    return events_ & other.events_;
  }

  template <typename... Events>
  bool OneOf(Events... events) const noexcept {
    if ((Contains(events) || ...)) {
      return true;
    }
    return false;
  }

  template <typename... Events>
  bool AllOf(Events... events) const noexcept {
    if ((Contains(events) && ...)) {
      return true;
    }
    return false;
  }

  ReceiveEvents& Add(ReceiveEvents other) noexcept {
    events_ |= other.events_;
    return *this;
  }

  ReceiveEvents& Remove(ReceiveEvents other) noexcept {
    events_ &= ~other.events_;
    return *this;
  }
};
}  // namespace pedronet

PEDROLIB_CLASS_FORMATTER(pedronet::SelectEvents);
#endif  // PEDRONET_EVENT_H