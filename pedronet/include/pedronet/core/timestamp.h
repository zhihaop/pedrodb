#ifndef PEDRONET_CORE_TIMESTAMP_H
#define PEDRONET_CORE_TIMESTAMP_H

#include "comparable.h"
#include "duration.h"

#include <limits>

namespace pedronet {
namespace core {
struct Timestamp : public Comparable<Timestamp> {
  int64_t usecs{};

  Timestamp() = default;
  Timestamp(int64_t usecs) : usecs(usecs) {}

  static Timestamp Now();

  static Timestamp Max() { return {std::numeric_limits<int64_t>::max()}; }

  static Timestamp Min() { return {0}; }

  static int Compare(const Timestamp &p, const Timestamp &q) noexcept {
    return p.usecs == q.usecs ? 0 : p.usecs < q.usecs ? -1 : 1;
  }

  Duration operator-(const Timestamp &other) const noexcept {
    return {usecs - other.usecs};
  }

  Timestamp operator+(const Duration &d) const noexcept {
    return {usecs + d.usecs};
  }

  Timestamp operator-(const Duration &d) const noexcept {
    return {usecs - d.usecs};
  }
};
} // namespace core
} // namespace pedronet

#endif // TIMESTAMP