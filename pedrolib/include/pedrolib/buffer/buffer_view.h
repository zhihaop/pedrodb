#ifndef PEDROLIB_BUFFER_BUFFER_VIEW_H
#define PEDROLIB_BUFFER_BUFFER_VIEW_H

#include "pedrolib/buffer/buffer.h"
#include <string>

namespace pedrolib {
class BufferView final : public Buffer {
  const char *data_{};
  size_t size_{};
  size_t read_index_{};

public:
  BufferView(const char *data, size_t size) : data_(data), size_(size) {}
  explicit BufferView(const char *data) : data_(data), size_(::strlen(data)) {}
  explicit BufferView(const std::string &s)
      : data_(s.data()), size_(s.size()) {}
  size_t ReadableBytes() override { return size_ - read_index_; }
  size_t WritableBytes() override { return 0; }
  void EnsureWriteable(size_t) override {}
  size_t Capacity() override { return size_; }
  size_t Append(const char *data, size_t n) override { return 0; }
  size_t Retrieve(char *data, size_t n) override;
  void Retrieve(size_t size) override {
    read_index_ = std::min(size_, read_index_ + size_);
  }

  void Append(size_t size) override {}
  void Reset() override { read_index_ = size_; }
  ssize_t Append(File *source) override { return 0; }
  ssize_t Retrieve(File *target) override;

  size_t Append(Buffer *buffer) override { return 0; }

  size_t Retrieve(Buffer *buffer) override;

  const char *ReadIndex() override { return data_ + read_index_; }
  char *WriteIndex() override { return const_cast<char *>(data_ + size_); }
  size_t Retrieve(std::string_view *sv, size_t n) noexcept {
    n = std::min(ReadableBytes(), n);
    *sv = std::string_view{data_ + read_index_, n};
    Retrieve(n);
    return n;
  }
};
} // namespace pedrolib
#endif // PEDROLIB_BUFFER_BUFFER_VIEW_H
