#ifndef PEDRONET_BUFFER_BUFFER_VIEW_H
#define PEDRONET_BUFFER_BUFFER_VIEW_H

#include "pedronet/buffer/buffer.h"
#include "pedronet/socket.h"

namespace pedronet {
class BufferView : public Buffer {
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
  size_t Retrieve(char *data, size_t n) override {
    size_t w = std::min(n, ReadableBytes());
    size_t target = read_index_ + w;
    while (read_index_ != target) {
      *(data++) = data_[read_index_++];
    }
    return w;
  }
  void Retrieve(size_t size) override {
    read_index_ = std::min(size_, read_index_ + size_);
  }

  void Append(size_t size) override {}
  void Reset() override { read_index_ = size_; }
  ssize_t Append(Socket *source) override { return 0; }
  ssize_t Retrieve(Socket *target) override {
    ssize_t w = target->Write(data_ + read_index_, ReadableBytes());
    if (w > 0) {
      Retrieve(w);
    }
    return w;
  }

  size_t Append(Buffer *buffer) override { return 0; }

  size_t Retrieve(Buffer *buffer) override {
    size_t w = buffer->Append(data_ + read_index_, ReadableBytes());
    Retrieve(w);
    return w;
  }

  size_t ReadIndex() override { return read_index_; }
  size_t WriteIndex() override { return size_; }
  const char &Get(size_t index) const override { return data_[index]; }
};
} // namespace pedronet
#endif // PEDRONET_BUFFER_BUFFER_VIEW_H
