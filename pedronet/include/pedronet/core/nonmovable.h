#ifndef PEDRONET_CORE_NONMOVEABLE_H
#define PEDRONET_CORE_NONMOVEABLE_H

namespace pedronet::core {

struct nonmovable {
  nonmovable() = default;
  ~nonmovable() = default;
  nonmovable(nonmovable &&) noexcept = delete;
  nonmovable &operator=(nonmovable &&) noexcept = delete;
};

} // namespace pedronet::core
#endif // PEDRONET_CORE_NONMOVEABLE_H