#ifndef PEDROLIB_BUFFER_BUFFER_H
#define PEDROLIB_BUFFER_BUFFER_H

#include "pedrolib/file/file.h"
#include <cstdio>
#include <cstring>
#include <string_view>

namespace pedrolib {

template <typename T> T htobe(T value) {
  if constexpr (sizeof(T) == 64) {
    return htobe64(value);
  }
  if constexpr (sizeof(T) == 32) {
    return htobe32(value);
  }
  if constexpr (sizeof(T) == 16) {
    return htobe16(value);
  }
  return value;
}

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
  virtual ssize_t Append(File *source) = 0;
  virtual ssize_t Retrieve(File *target) = 0;
  virtual size_t Append(Buffer *buffer) = 0;
  virtual size_t Retrieve(Buffer *buffer) = 0;

  size_t Retrieve(std::string *s, size_t n) {
    s->resize(n);
    return Retrieve(s->data(), n);
  }

  size_t Append(std::string_view s) { return Append(s.data(), s.size()); }

  template <typename Int> bool RetrieveInt(Int *value) {
    if (ReadableBytes() < sizeof(Int)) {
      return false;
    }
    Retrieve(reinterpret_cast<char *>(value), sizeof(Int));
    *value = htobe(*value);
    return true;
  }

  template <typename Int> bool AppendInt(Int value) {
    if (WritableBytes() < sizeof(Int)) {
      return false;
    }
    value = htobe(value);
    Append(reinterpret_cast<const char *>(&value), sizeof(Int));
    return true;
  }
};

} // namespace pedrolib

#endif // PEDROLIB_BUFFER_BUFFER_H