#ifndef PEDRONET_CORE_COMPARABLE_H
#define PEDRONET_CORE_COMPARABLE_H

namespace pedronet {
namespace core {
template <typename T> struct Comparable {
  friend bool operator<(const T &p, const T &q) noexcept {
    return T::Compare(p, q) < 0;
  }
  friend bool operator<=(const T &p, const T &q) noexcept {
    return T::Compare(p, q) <= 0;
  }
  friend bool operator==(const T &p, const T &q) noexcept {
    return T::Compare(p, q) == 0;
  }
  friend bool operator>=(const T &p, const T &q) noexcept {
    return T::Compare(p, q) >= 0;
  }
  friend bool operator>(const T &p, const T &q) noexcept {
    return T::Compare(p, q) > 0;
  }
  friend bool operator!=(const T &p, const T &q) noexcept {
    return T::Compare(p, q) != 0;
  }
};

} // namespace core
} // namespace pedronet

#endif // PEDRONET_CORE_COMPARABLE_H