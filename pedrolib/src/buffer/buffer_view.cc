#include "pedrolib/buffer/buffer_view.h"

namespace pedrolib {

size_t BufferView::Retrieve(Buffer* buffer) {
  size_t w = buffer->Append(data_ + read_index_, ReadableBytes());
  Retrieve(w);
  return w;
}

ssize_t BufferView::Retrieve(File* target) {
  ssize_t w = target->Write(data_ + read_index_, ReadableBytes());
  if (w > 0) {
    Retrieve(w);
  }
  return w;
}
size_t BufferView::Retrieve(char* data, size_t n) {
  size_t w = std::min(n, ReadableBytes());
  memcpy(data, data_ + read_index_, w);
  Retrieve(w);
  return w;
}
}  // namespace pedrolib