#ifndef PEDRONET_BUFFER_BUFFER_H
#define PEDRONET_BUFFER_BUFFER_H
#include "pedronet/core/noncopyable.h"
#include "pedronet/socket.h"

namespace pedronet {

struct Buffer {
  virtual size_t ReadableBytes() = 0;
  virtual size_t WritableBytes() = 0;
  virtual void EnsureWriteable(size_t) = 0;

  virtual size_t Capacity() = 0;

  virtual void Retrieve(size_t) = 0;
  virtual void Append(size_t) = 0;
  virtual void Reset() = 0;

  virtual size_t Peek(char *data, size_t n) = 0;
  virtual size_t ReadIndex() = 0;
  virtual size_t WriteIndex() = 0;
  virtual size_t Find(std::string_view sv) = 0;
  virtual size_t Append(const char *data, size_t n) = 0;
  virtual size_t Retrieve(char *data, size_t n) = 0;
  virtual ssize_t Append(Socket *source) = 0;
  virtual ssize_t Retrieve(Socket *target) = 0;
  virtual size_t Append(Buffer *buffer) = 0;
  virtual size_t Retrieve(Buffer *buffer) = 0;
};

} // namespace pedronet

#endif // PEDRONET_BUFFER_BUFFER_H