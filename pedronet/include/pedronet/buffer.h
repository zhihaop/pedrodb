#ifndef PEDRONET_BUFFER_H
#define PEDRONET_BUFFER_H
#include "pedronet/core/noncopyable.h"
#include "pedronet/socket.h"
#include <algorithm>
#include <vector>

#include <sys/uio.h>

namespace pedronet {

struct Buffer {

  class Iterator {
    const Buffer *buffer_;
    size_t index_;

  public:
    typedef std::random_access_iterator_tag iterator_category;
    typedef char value_type;
    typedef char *pointer;
    typedef char &reference;
    typedef ptrdiff_t difference_type;

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

    Buffer::Iterator &operator++() {
      index_++;
      return *this;
    }

    Buffer::Iterator operator++(int) const {
      Buffer::Iterator that = *this;
      ++that;
      return that;
    }

    Buffer::Iterator &operator--() {
      index_--;
      return *this;
    }

    Buffer::Iterator operator--(int) const {
      Buffer::Iterator that = *this;
      --that;
      return that;
    }

    Buffer::Iterator &operator+=(size_t index) {
      index_ += index;
      return *this;
    }

    Buffer::Iterator &operator-=(size_t index) {
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

class ArrayBuffer : public Buffer {
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
      std::copy(buf_.data() + read_index_, buf_.data() + write_index_,
                buf_.data());
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
    io[0].iov_base = buf_.data() + write_index_;
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
  const char &Get(size_t index) const override { return buf_[index]; }
};
} // namespace pedronet

#endif // PEDRONET_BUFFER_H