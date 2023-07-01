#ifndef PEDRONET_CORE_TIMESTAMP_H
#define PEDRONET_CORE_TIMESTAMP_H

#include "comparable.h"
#include "duration.h"

#include <limits>

namespace pedronet::core {
struct Timestamp : public Comparable<Timestamp> {
  int64_t usecs{};

  Timestamp() = default;
  explicit Timestamp(int64_t usecs) : usecs(usecs) {}

  static Timestamp Now();

  static Timestamp Max() {
    return Timestamp{std::numeric_limits<int64_t>::max()};
  }

  static Timestamp Min() { return Timestamp{0}; }

  static int Compare(const Timestamp &p, const Timestamp &q) noexcept {
    return p.usecs == q.usecs ? 0 : p.usecs < q.usecs ? -1 : 1;
  }

  Duration operator-(const Timestamp &other) const noexcept {
    return Duration{usecs - other.usecs};
  }

  Timestamp operator+(const Duration &d) const noexcept {
    return Timestamp{usecs + d.usecs};
  }

  Timestamp operator-(const Duration &d) const noexcept {
    return Timestamp{usecs - d.usecs};
  }
};
} // namespace pedronet::core

#endif // PEDRONET_CORE_TIMESTAMP_H