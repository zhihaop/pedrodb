#include "pedrolib/buffer/array_buffer.h"

namespace pedrolib {

void ArrayBuffer::EnsureWritable(size_t n, bool fixed) {
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

  if (fixed) {
    buf_.resize(size);
  } else {
    buf_.resize(size << 1);
  }
}

ssize_t ArrayBuffer::Append(File* source) {
  const size_t kMaxReadBytes = 65536;
  
  if (WritableBytes() >= kMaxReadBytes) {
    ssize_t r = source->Read(WriteIndex(), kMaxReadBytes);
    if (r > 0) {
      Append(r);
    }
    return r;
  }

  size_t writable = WritableBytes();
  char* buf = reinterpret_cast<char*>(alloca(kMaxReadBytes - writable));
  std::string_view io_vector[2] = {{WriteIndex(), writable},
                                   {buf, kMaxReadBytes - writable}};

  ssize_t r = source->Readv(io_vector, 2);
  if (r <= 0) {
    return r;
  }

  if (r <= writable) {
    Append(r);
    return r;
  }

  Append(writable);

  EnsureWritable(r - writable, true);
  Append(buf, r - writable);
  return r;
}

void ArrayBuffer::Append(const char* data, size_t n) {
  EnsureWritable(n, false);
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
  EnsureWritable(r, false);
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