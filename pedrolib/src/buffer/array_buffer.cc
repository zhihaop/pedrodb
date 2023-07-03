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
  buf_.resize(buf_.size() + delta);
}
ssize_t ArrayBuffer::Append(File *source) {
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

  EnsureWriteable(r);
  Append(writable);
  Append(buf, r - writable);
  return r;
}
size_t ArrayBuffer::Find(std::string_view sv) {
  std::string_view view{buf_.data() + read_index_, ReadableBytes()};
  size_t n = view.find(sv);
  if (n == std::string_view::npos) {
    return n;
  }
  return n + read_index_;
}
size_t ArrayBuffer::Append(const char *data, size_t n) {
  EnsureWriteable(n);
  memcpy(buf_.data() + write_index_, data, n);
  Append(n);
  return n;
}
size_t ArrayBuffer::Retrieve(char *data, size_t n) {
  n = std::min(n, ReadableBytes());
  memcpy(data, buf_.data() + read_index_, n);
  Retrieve(n);
  return n;
}
ssize_t ArrayBuffer::Retrieve(File *target) {
  ssize_t w = target->Write(buf_.data() + read_index_, ReadableBytes());
  if (w > 0) {
    Retrieve(w);
  }
  return w;
}
size_t ArrayBuffer::Append(Buffer *buffer) {
  EnsureWriteable(buffer->ReadableBytes());
  size_t r = buffer->Retrieve(buf_.data() + write_index_, WritableBytes());
  Append(r);
  return r;
}
size_t ArrayBuffer::Peek(char *data, size_t n) {
  n = std::min(n, ReadableBytes());
  memcpy(data, buf_.data() + read_index_, n);
  return n;
}
size_t ArrayBuffer::Retrieve(Buffer *buffer) {
  size_t w = buffer->Append(buf_.data() + read_index_, ReadableBytes());
  Retrieve(w);
  return w;
}
void ArrayBuffer::Retrieve(size_t n) {
  read_index_ = std::min(read_index_ + n, write_index_);
  if (read_index_ == write_index_) {
    Reset();
  }
}
// namespace pedronet
} // namespace pedrolib