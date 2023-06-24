#ifndef PEDRONET_CORE_NONCOPYABLE_H
#define PEDRONET_CORE_NONCOPYABLE_H

namespace pedronet {

namespace core {

struct noncopyable {
  noncopyable() = default;
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator=(const noncopyable &) = delete;
};

} // namespace core

} // namespace pedronet
#endif // PEDRONET_CORE_NONCOPYABLE_H