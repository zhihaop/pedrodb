#ifndef PEDROLIB_NONCOPYABLE_H
#define PEDROLIB_NONCOPYABLE_H

namespace pedrolib {

struct noncopyable {
  noncopyable() = default;
  ~noncopyable() = default;
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator=(const noncopyable &) = delete;
};

} // namespace pedrolib
#endif // PEDROLIB_NONCOPYABLE_H