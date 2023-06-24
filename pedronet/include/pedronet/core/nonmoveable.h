#ifndef PEDRONET_CORE_NONMOVEABLE_H
#define PEDRONET_CORE_NONMOVEABLE_H

namespace pedronet {

namespace core {

struct nonmoveable {
  nonmoveable() = default;
  nonmoveable(nonmoveable &&) noexcept = delete;
  nonmoveable &operator=(nonmoveable &&) noexcept = delete;
};

} // namespace core

} // namespace pedronet
#endif // PEDRONET_CORE_NONMOVEABLE_H