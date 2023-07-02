#include "pedronet/buffer/buffer_view.h"
#include "pedronet/socket.h"

namespace pedronet {

size_t BufferView::Retrieve(Buffer *buffer) {
  size_t w = buffer->Append(data_ + read_index_, ReadableBytes());
  Retrieve(w);
  return w;
}
size_t BufferView::Find(std::string_view sv) {
  std::string_view view{data_ + read_index_, size_};
  size_t n = view.find(sv);
  if (n == std::string_view::npos) {
    return n;
  }
  return n + read_index_;
}
size_t BufferView::Peek(char *data, size_t n) {
  n = std::min(n, ReadableBytes());
  memcpy(data, data_ + read_index_, n);
  return n;
}
ssize_t BufferView::Retrieve(Socket *target) {
  ssize_t w = target->Write(data_ + read_index_, ReadableBytes());
  if (w > 0) {
    Retrieve(w);
  }
  return w;
}
size_t BufferView::Retrieve(char *data, size_t n) {
  size_t w = std::min(n, ReadableBytes());
  memcpy(data, data_ + read_index_, w);
  Retrieve(w);
  return w;
}
} // namespace pedronet