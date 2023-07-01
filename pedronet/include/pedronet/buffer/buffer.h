#ifndef PEDRONET_BUFFER_BUFFER_H
#define PEDRONET_BUFFER_BUFFER_H
#include "pedronet/core/noncopyable.h"
#include "pedronet/socket.h"

namespace pedronet {

struct Buffer {

  class Iterator {
    const Buffer *buffer_;
    size_t index_;

  public:
    [[maybe_unused]] typedef std::random_access_iterator_tag iterator_category;
    [[maybe_unused]] typedef char value_type;
    [[maybe_unused]] typedef char *pointer;
    [[maybe_unused]] typedef char &reference;
    [[maybe_unused]] typedef ptrdiff_t difference_type;

    Iterator(const Buffer::Iterator &other) = default;
    Buffer::Iterator &operator=(const Buffer::Iterator &other) = default;

    bool operator==(const Buffer::Iterator &other) const noexcept {
      return buffer_ == other.buffer_ && index_ == other.index_;
    }

    bool operator!=(const Buffer::Iterator &other) const noexcept {
      return !(*this == other);
    }

    const char &operator*() const noexcept { return buffer_->Get(index_); }
    const char &operator[](size_t index) const noexcept {
      return buffer_->Get(index_ + index);
    }

    Buffer::Iterator &operator++() noexcept {
      index_++;
      return *this;
    }

    Buffer::Iterator operator++(int) const noexcept {
      Buffer::Iterator that = *this;
      ++that;
      return that;
    }

    Buffer::Iterator &operator--() noexcept {
      index_--;
      return *this;
    }

    Buffer::Iterator operator--(int) const noexcept {
      Buffer::Iterator that = *this;
      --that;
      return that;
    }

    Buffer::Iterator &operator+=(size_t index) noexcept {
      index_ += index;
      return *this;
    }

    Buffer::Iterator &operator-=(size_t index) noexcept {
      index_ -= index;
      return *this;
    }

    Buffer::Iterator operator+(size_t index) const noexcept {
      Buffer::Iterator that = *this;
      that += index;
      return that;
    }

    Buffer::Iterator operator-(size_t index) const noexcept {
      Buffer::Iterator that = *this;
      that -= index;
      return that;
    }

    ptrdiff_t operator-(const Buffer::Iterator &other) const noexcept {
      return index_ - other.index_;
    }

    Iterator(const Buffer *buffer, size_t index)
        : buffer_(buffer), index_(index) {}
    ~Iterator() = default;
  };

  virtual size_t ReadableBytes() = 0;
  virtual size_t WritableBytes() = 0;
  virtual void EnsureWriteable(size_t) = 0;

  virtual size_t Capacity() = 0;

  virtual void Retrieve(size_t) = 0;
  virtual void Append(size_t) = 0;
  virtual void Reset() = 0;

  virtual const char &Get(size_t) const = 0;
  virtual size_t ReadIndex() = 0;
  virtual size_t WriteIndex() = 0;
  virtual size_t Append(const char *data, size_t n) = 0;
  virtual size_t Retrieve(char *data, size_t n) = 0;
  virtual ssize_t Append(Socket *source) = 0;
  virtual ssize_t Retrieve(Socket *target) = 0;
  virtual size_t Append(Buffer *buffer) = 0;
  virtual size_t Retrieve(Buffer *buffer) = 0;

  Iterator GetIterator(size_t index) const noexcept { return {this, index}; }
};

} // namespace pedronet

#endif // PEDRONET_BUFFER_BUFFER_H