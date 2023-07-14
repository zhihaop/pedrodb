#ifndef PEDROLIB_NONMOVABLE_H
#define PEDROLIB_NONMOVABLE_H

namespace pedrolib {

struct nonmovable {
  nonmovable() = default;
  ~nonmovable() = default;
  nonmovable(nonmovable&&) noexcept = delete;
  nonmovable& operator=(nonmovable&&) noexcept = delete;
};
}  // namespace pedrolib
#endif  // PEDROLIB_NONMOVABLE_H