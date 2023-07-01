#include "pedronet/buffer/array_buffer.h"

namespace pedronet {

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
ssize_t ArrayBuffer::Append(Socket *source) {
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
} // namespace pedronet
} // namespace pedronet