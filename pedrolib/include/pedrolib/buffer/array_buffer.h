#ifndef PEDROLIB_BUFFER_ARRAY_BUFFER_H
#define PEDROLIB_BUFFER_ARRAY_BUFFER_H

#include <cstring>
#include <vector>
#include "pedrolib/buffer/buffer.h"
#include "pedrolib/file/file.h"

namespace pedrolib {
class ArrayBuffer final {
  static const size_t kInitialSize = 1024;

  std::vector<char> buf_;
  size_t read_index_{};
  size_t write_index_{};

 public:
  explicit ArrayBuffer(size_t capacity) : buf_(capacity) {}
  ArrayBuffer() : ArrayBuffer(kInitialSize) {}

  [[nodiscard]] size_t Capacity() const noexcept { return buf_.size(); }

  [[nodiscard]] size_t ReadableBytes() const noexcept {
    return write_index_ - read_index_;
  }

  void Append(size_t n) {
    write_index_ = std::min(write_index_ + n, buf_.size());
  }

  [[nodiscard]] size_t WritableBytes() const noexcept {
    return buf_.size() - write_index_;
  }

  void Retrieve(size_t n);

  void Reset() { read_index_ = write_index_ = 0; }

  void Append(const char* data, size_t n);

  size_t Retrieve(char* data, size_t n);

  void EnsureWriteable(size_t n, bool fixed = true);

  ssize_t Append(File* source);

  ssize_t Retrieve(File* target);

  void Append(ArrayBuffer* buffer);

  void Retrieve(ArrayBuffer* buffer);

  const char* ReadIndex() { return buf_.data() + read_index_; }

  char* WriteIndex() { return buf_.data() + write_index_; }
};
}  // namespace pedrolib

#endif  // PEDROLIB_BUFFER_ARRAY_BUFFER_H
