#ifndef PEDROLIB_COMPARABLE_H
#define PEDROLIB_COMPARABLE_H

namespace pedrolib {
template <typename T>
struct Comparable {
  friend bool operator<(const T& p, const T& q) noexcept {
    return T::Compare(p, q) < 0;
  }
  friend bool operator<=(const T& p, const T& q) noexcept {
    return T::Compare(p, q) <= 0;
  }
  friend bool operator==(const T& p, const T& q) noexcept {
    return T::Compare(p, q) == 0;
  }
  friend bool operator>=(const T& p, const T& q) noexcept {
    return T::Compare(p, q) >= 0;
  }
  friend bool operator>(const T& p, const T& q) noexcept {
    return T::Compare(p, q) > 0;
  }
  friend bool operator!=(const T& p, const T& q) noexcept {
    return T::Compare(p, q) != 0;
  }
};

}  // namespace pedrolib

#endif  // PEDROLIB_COMPARABLE_H