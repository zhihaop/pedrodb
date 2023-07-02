#ifndef PEDRONET_BUFFER_ARRAY_BUFFER_H
#define PEDRONET_BUFFER_ARRAY_BUFFER_H

#include "pedronet/buffer/buffer.h"
#include <cstring>
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

  void Retrieve(size_t n) override;

  void Reset() override { read_index_ = write_index_ = 0; }

  size_t Append(const char *data, size_t n) override;

  size_t Retrieve(char *data, size_t n) override;

  void EnsureWriteable(size_t n) override;

  ssize_t Append(Socket *source) override;

  ssize_t Retrieve(Socket *target) override;

  size_t Append(Buffer *buffer) override;

  size_t Retrieve(Buffer *buffer) override;

  size_t ReadIndex() override { return read_index_; }
  size_t WriteIndex() override { return write_index_; }

  size_t Peek(char *data, size_t n) override;

  size_t Find(std::string_view sv) override;
};
} // namespace pedronet

#endif // PEDRONET_BUFFER_ARRAY_BUFFER_H
