#ifndef PEDRONET_CORE_STATIC_VECTOR_H
#define PEDRONET_CORE_STATIC_VECTOR_H

#include "pedronet/core/noncopyable.h"
#include "pedronet/core/nonmovable.h"
#include <cstdlib>
#include <memory>

namespace pedronet::core {
template <typename T> class StaticVector : nonmovable, noncopyable {

  struct Deleter {
    void operator()(void *p) const noexcept { std::free(p); }
  };

  size_t size_;
  size_t capacity_;
  std::unique_ptr<T, Deleter> data_;

public:
  explicit StaticVector(size_t capacity) : size_(0), capacity_(capacity) {
    auto ptr = std::aligned_alloc(alignof(T), capacity * sizeof(T));
    data_ = decltype(data_)(static_cast<T *>(ptr), Deleter{});
  }

  template <typename... Args> T &emplace_back(Args &&...args) {
    return *new (&data()[size_++]) T(std::forward<Args>(args)...);
  }

  void pop_back() { std::destroy_at(&data()[--size_]); }

  T *data() noexcept { return data_.get(); }
  T *begin() noexcept { return data_.get(); }
  T *end() noexcept { return begin() + size_; }

  size_t size() const noexcept { return size_; }

  size_t capacity() const noexcept { return capacity_; }

  bool empty() const noexcept { return size_ == 0; }

  void clear() {
    while (!empty()) {
      pop_back();
    }
  }

  T &operator[](size_t index) noexcept { return data_.get()[index]; }

  const T &operator[](size_t index) const noexcept {
    return data_.get()[index];
  }
};
} // namespace pedronet::core
#endif // PEDRONET_CORE_STATIC_VECTOR_H
