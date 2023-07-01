#ifndef PEDRONET_BUFFER_ARRAY_BUFFER_H
#define PEDRONET_BUFFER_ARRAY_BUFFER_H

#include "pedronet/buffer/buffer.h"
#include <vector>

namespace pedronet {
class ArrayBuffer final : public Buffer {
  static const size_t kInitialSize = 1024;

  std::vector<char> buf_;
  size_t read_index_{};
  size_t write_index_{};

public:
  explicit ArrayBuffer(size_t capacity) : buf_(capacity) {}

  ArrayBuffer() : ArrayBuffer(kInitialSize) {}

  size_t Capacity() override { return buf_.size(); }

  size_t ReadableBytes() override { return write_index_ - read_index_; }

  void Append(size_t n) override {
    write_index_ = std::min(write_index_ + n, buf_.size());
  }

  size_t WritableBytes() override { return buf_.size() - write_index_; }

  void Retrieve(size_t n) override {
    read_index_ = std::min(read_index_ + n, write_index_);
    if (read_index_ == write_index_) {
      Reset();
    }
  }

  void Reset() override { read_index_ = write_index_ = 0; }

  size_t Append(const char *data, size_t n) override {
    n = std::min(n, WritableBytes());
    memcpy(buf_.data() + write_index_, data, n);
    Append(n);
    return n;
  }

  size_t Retrieve(char *data, size_t n) override {
    n = std::min(n, ReadableBytes());
    memcpy(data, buf_.data() + read_index_, n);
    Retrieve(n);
    return n;
  }

  void EnsureWriteable(size_t n) override;

  ssize_t Append(Socket *source) override;

  ssize_t Retrieve(Socket *target) override {
    ssize_t w = target->Write(buf_.data() + read_index_, ReadableBytes());
    if (w > 0) {
      Retrieve(w);
    }
    return w;
  }

  size_t Append(Buffer *buffer) override {
    size_t r = buffer->Retrieve(buf_.data() + write_index_, WritableBytes());
    Append(r);
    return r;
  }

  size_t Retrieve(Buffer *buffer) override {
    size_t w = buffer->Append(buf_.data() + read_index_, ReadableBytes());
    Retrieve(w);
    return w;
  }

  size_t ReadIndex() override { return read_index_; }
  size_t WriteIndex() override { return write_index_; }

  size_t Peek(char *data, size_t n) override {
    n = std::min(n, ReadableBytes());
    memcpy(data, buf_.data() + read_index_, n);
    return n;
  }

  size_t Find(std::string_view sv) override;
};
} // namespace pedronet

#endif // PEDRONET_BUFFER_ARRAY_BUFFER_H
