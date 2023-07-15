#include "pedrolib/buffer/array_buffer.h"

namespace pedrolib {

void ArrayBuffer::EnsureWriteable(size_t n) {
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
  size_t size = buf_.size() + delta;
  buf_.resize(size << 1);
}
ssize_t ArrayBuffer::Append(File* source) {
  char buf[65535];
  size_t writable = WritableBytes();
  std::string_view views[2] = {{buf_.data() + write_index_, writable},
                               {buf, sizeof(buf)}};

  const int cnt = (writable < sizeof(buf)) ? 2 : 1;
  ssize_t r = source->Readv(views, cnt);
  if (r <= 0) {
    return r;
  }

  if (r <= writable) {
    Append(r);
    return r;
  }

  Append(writable);

  EnsureWriteable(r - writable);
  Append(buf, r - writable);
  return r;
}

void ArrayBuffer::Append(const char* data, size_t n) {
  EnsureWriteable(n);
  memcpy(WriteIndex(), data, n);
  Append(n);
}

size_t ArrayBuffer::Retrieve(char* data, size_t n) {
  n = std::min(n, ReadableBytes());
  memcpy(data, ReadIndex(), n);
  Retrieve(n);
  return n;
}

ssize_t ArrayBuffer::Retrieve(File* target) {
  ssize_t w = target->Write(ReadIndex(), ReadableBytes());
  if (w > 0) {
    Retrieve(w);
  }
  return w;
}

void ArrayBuffer::Append(ArrayBuffer* buffer) {
  size_t r = buffer->ReadableBytes();
  EnsureWriteable(r);
  buffer->Retrieve(WriteIndex(), r);
  Append(r);
}

void ArrayBuffer::Retrieve(ArrayBuffer* buffer) {
  buffer->Append(ReadIndex(), ReadableBytes());
  Retrieve(ReadableBytes());
}

void ArrayBuffer::Retrieve(size_t n) {
  read_index_ = std::min(read_index_ + n, write_index_);
  if (read_index_ == write_index_) {
    Reset();
  }
}
// namespace pedronet
}  // namespace pedrolib