#ifndef PEDRONET_BUFFER_H
#define PEDRONET_BUFFER_H
#include "pedronet/core/noncopyable.h"
#include "pedronet/socket.h"
#include <algorithm>
#include <vector>

#include <sys/uio.h>

namespace pedronet {
struct Buffer {
  virtual size_t ReadableBytes() = 0;
  virtual size_t WritableBytes() = 0;
  virtual void EnsureWriteable(size_t) = 0;

  virtual size_t Capacity() = 0;
  virtual const char *ReadIndex() = 0;
  virtual char *WriteIndex() = 0;

  virtual char *Data() = 0;
  virtual size_t Append(const char *data, size_t n) = 0;
  virtual size_t Retrieve(char *data, size_t n) = 0;
  virtual void Retrieve(size_t) = 0;
  virtual void Append(size_t) = 0;
  virtual void Reset() = 0;
  virtual ssize_t Append(Socket *source) = 0;
  virtual ssize_t Retrieve(Socket *target) = 0;
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

  const char *ReadIndex() override { return buf_.data() + read_index_; }

  char *WriteIndex() override { return buf_.data() + write_index_; }

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
    size_t next_index = write_index_ + n;
    for (size_t i = write_index_; i < next_index; ++i) {
      buf_[i] = *(data++);
    }
    Append(n);
    return n;
  }

  size_t Retrieve(char *data, size_t n) override {
    n = std::min(n, ReadableBytes());
    size_t next_index = read_index_ + n;
    for (size_t i = read_index_; i < next_index; ++i) {
      *(data++) = buf_[i];
    }
    Retrieve(n);
    return n;
  }

  void EnsureWriteable(size_t n) override {
    size_t w = WritableBytes();
    if (n <= w) {
      return;
    }

    if (read_index_ + w > n) {
      size_t r = ReadableBytes();
      std::copy(buf_.begin() + read_index_, buf_.begin() + write_index_,
                buf_.begin());
      read_index_ = 0;
      write_index_ = read_index_ + r;
      return;
    }
    size_t delta = n - w;
    buf_.resize(buf_.size() + delta);
  }

  ssize_t Append(Socket *source) override {
    char buf[65535];
    std::array<struct iovec, 2> io{};
    size_t writable = WritableBytes();
    io[0].iov_base = WriteIndex();
    io[0].iov_len = writable;
    io[1].iov_base = buf;
    io[1].iov_len = sizeof(buf);

    const int cnt = (writable < sizeof(buf)) ? 2 : 1;
    ssize_t r = ::readv(source->Descriptor(), io.data(), cnt);
    if (r <= 0) {
      return r;
    }

    if (r <= writable) {
      Append(r);
      return r;
    }

    EnsureWriteable(r);
    Append(writable);
    Append(buf, r - writable);
    return r;
  }

  ssize_t Retrieve(Socket *target) override {
    ssize_t w = ::write(target->Descriptor(), ReadIndex(), ReadableBytes());
    if (w > 0) {
      Retrieve(w);
    }
    return w;
  }
};
} // namespace pedronet

#endif // PEDRONET_BUFFER_H