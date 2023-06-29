#ifndef PEDRONET_CORE_NONCOPYABLE_H
#define PEDRONET_CORE_NONCOPYABLE_H

namespace pedronet::core {

struct noncopyable {
  noncopyable() = default;
  ~noncopyable() = default;
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator=(const noncopyable &) = delete;
};

} // namespace pedronet::core
#endif // PEDRONET_CORE_NONCOPYABLE_H