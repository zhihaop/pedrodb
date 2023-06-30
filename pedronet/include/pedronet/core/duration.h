#ifndef PEDRONET_CORE_DURATION_H
#define PEDRONET_CORE_DURATION_H

#include "comparable.h"

#include <chrono>

namespace pedronet::core {

constexpr inline static int64_t kMilliseconds = 1000;
constexpr inline static int64_t kMicroseconds = kMilliseconds * 1000;
constexpr inline static int64_t kNanoseconds = kMicroseconds * 1000;

struct Duration : public Comparable<Duration> {
  int64_t usecs{};

  Duration() = default;
  Duration(const Duration &other) : usecs(other.usecs) {}
  
  template <typename Rep, typename Period>
  Duration(std::chrono::duration<Rep, Period> other)
      : usecs(std::chrono::duration_cast<std::chrono::microseconds>(other)
                  .count()) {}
  
  explicit Duration(int64_t usecs) : usecs(usecs) {}

  static int Compare(const Duration &p, const Duration &q) noexcept {
    return p.usecs == q.usecs ? 0 : p.usecs < q.usecs ? -1 : 1;
  }

  static Duration Zero() { return {}; }

  static Duration Seconds(int32_t secs) {
    return Duration{secs * kMicroseconds};
  }

  static Duration Milliseconds(int64_t ms) {
    return Duration{ms * kMilliseconds};
  }

  static Duration Microseconds(int64_t us) { return Duration{us}; }

  int64_t Microseconds() const noexcept { return usecs; }

  int64_t Milliseconds() const noexcept { return usecs / 1000; }

  Duration &operator=(const Duration &other) noexcept {
    usecs = other.usecs;
    return *this;
  }

  template <typename Rep, typename Period>
  Duration &operator=(std::chrono::duration<Rep, Period> other) noexcept {
    usecs =
        std::chrono::duration_cast<std::chrono::microseconds>(other).count();
    return *this;
  }
};
} // namespace pedronet::core

#endif // PEDRONET_CORE_DURATION_H