#ifndef PEDROLIB_BUFFER_BUFFER_SLICE_H
#define PEDROLIB_BUFFER_BUFFER_SLICE_H

#include "pedrolib/buffer/buffer.h"
#include <string>

namespace pedrolib {
class BufferSlice final : public Buffer {
  char *data_;
  size_t size_;
  size_t read_index_;
  size_t write_index_;

public:
  BufferSlice(char *data, size_t size)
      : data_(data), size_(size), read_index_(0), write_index_(size) {}
  BufferSlice(void *data, size_t size)
      : data_(static_cast<char *>(data)), size_(size), read_index_(0),
        write_index_(size) {}
  explicit BufferSlice(char *data) : BufferSlice(data, ::strlen(data)) {}
  explicit BufferSlice(std::string &s) : BufferSlice(s.data(), s.size()) {}
  [[nodiscard]] size_t ReadableBytes() const noexcept override {
    return write_index_ - read_index_;
  }
  [[nodiscard]] size_t WritableBytes() const noexcept override {
    return size_ - write_index_;
  }
  void EnsureWriteable(size_t n) override {
    if (read_index_ + WritableBytes() < n) {
      return;
    }

    size_t r = write_index_ - read_index_;
    std::copy(data_ + read_index_, data_ + write_index_, data_);
    read_index_ = 0;
    write_index_ = r;
  }

  [[nodiscard]] size_t Capacity() const noexcept override { return size_; }

  size_t Append(const char *data, size_t n) override {
    n = std::min(n, WritableBytes());
    memcpy(data_ + write_index_, data, n);
    Append(n);
    return n;
  }

  size_t Retrieve(char *data, size_t n) override {
    n = std::min(n, ReadableBytes());
    memcpy(data, data_ + read_index_, n);
    Retrieve(n);
    return n;
  }

  void Retrieve(size_t n) override { read_index_ += n; }
  void Append(size_t n) override { write_index_ += n; }

  void Reset() override { read_index_ = write_index_ = 0; }

  ssize_t Append(File *source) override {
    ssize_t r = source->Read(data_ + write_index_, WritableBytes());
    if (r > 0) {
      Append(r);
    }
    return r;
  }

  ssize_t Retrieve(File *target) override {
    ssize_t w = target->Write(data_ + write_index_, WritableBytes());
    if (w > 0) {
      Retrieve(w);
    }
    return w;
  }

  size_t Append(Buffer *buffer) override {
    size_t r = buffer->Retrieve(data_ + write_index_, WritableBytes());
    Append(r);
    return r;
  }

  size_t Retrieve(Buffer *buffer) override {
    size_t w = buffer->Append(data_ + read_index_, ReadableBytes());
    Retrieve(w);
    return w;
  }

  const char *ReadIndex() override { return data_ + read_index_; }
  char *WriteIndex() override { return data_ + write_index_; }
};
} // namespace pedrolib

#endif // PEDROLIB_BUFFER_BUFFER_SLICE_H
