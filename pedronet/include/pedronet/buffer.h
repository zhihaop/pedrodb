#ifndef PEDRONET_BUFFER_H
#define PEDRONET_BUFFER_H
#include "core/noncopyable.h"
#include <algorithm>
#include <vector>

namespace pedronet {
struct Buffer {
  virtual size_t ReadableBytes() = 0;
  virtual size_t WritableBytes() = 0;
  virtual size_t EnsureWriteable(size_t) = 0;

  virtual size_t Capacity() = 0;
  virtual size_t ReadIndex() = 0;
  virtual size_t WriteIndex() = 0;

  virtual char *Data() = 0;
  virtual size_t Append(char *data, size_t n) = 0;
  virtual size_t Retrieve(char *data, size_t n) = 0;
  virtual void Retrieve(size_t) = 0;
  virtual void Append(size_t) = 0;

  template <class Reader> ssize_t Append(Reader *reader, size_t size) {
    ssize_t r =
        reader->Read(Data() + WriteIndex(), std::min(size, WritableBytes()));
    if (r > 0) {
      Append(r);
    }
    return r;
  }

  template <class Writer> ssize_t Retrieve(Writer *writer, size_t size) {
    ssize_t w =
        writer->Write(Data() + ReadIndex(), std::min(size, ReadableBytes()));
    if (w > 0) {
      Retrieve(w);
    }
    return w;
  }
};

class ArrayBuffer : public Buffer {
  static const size_t kInitialSize = 1024;

  std::vector<char> buf_;
  size_t read_index_{};
  size_t write_index_{};

public:
  explicit ArrayBuffer(size_t capacity) : buf_(capacity) {}

  ArrayBuffer() : ArrayBuffer(kInitialSize) {}

  size_t Capacity() override { return buf_.size(); }

  char *Data() override { return buf_.data(); }

  size_t ReadableBytes() override { return write_index_ - read_index_; }

  size_t ReadIndex() override { return read_index_; }

  size_t WriteIndex() override { return write_index_; }

  void Append(size_t n) override {
    write_index_ = std::min(write_index_ + n, buf_.size());
  }

  size_t WritableBytes() override { return buf_.size() - write_index_; }

  void Retrieve(size_t n) override {
    read_index_ = std::min(read_index_ + n, write_index_);
  }

  size_t Append(char *data, size_t n) override {
    n = std::min(n, WritableBytes());
    size_t next_index = write_index_ + n;
    for (size_t i = write_index_; i < next_index; ++i) {
      buf_[i] = *(data++);
    }
    write_index_ = next_index;
    return n;
  }

  size_t Retrieve(char *data, size_t n) override {
    n = std::min(n, ReadableBytes());
    size_t next_index = read_index_ + n;
    for (size_t i = read_index_; i < next_index; ++i) {
      *(data++) = buf_[i];
    }
    read_index_ = next_index;
    return n;
  }

  size_t EnsureWriteable(size_t n) override {
    if (n <= WritableBytes()) {
      return n;
    }

    size_t delta = n - WritableBytes();
    buf_.resize(buf_.size() + delta);
    return n;
  }
};
} // namespace pedronet

#endif // PEDRONET_BUFFER_H