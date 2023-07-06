#ifndef PEDROLIB_BUFFER_BUFFER_H
#define PEDROLIB_BUFFER_BUFFER_H

#include "pedrolib/file/file.h"
#include <cstdio>
#include <cstring>
#include <string_view>

namespace pedrolib {

template <typename T, typename E = std::enable_if<std::is_arithmetic_v<T>>>
T htobe(T value) {
  if constexpr (sizeof(T) == sizeof(uint64_t)) {
    return htobe64(value);
  }
  if constexpr (sizeof(T) == sizeof(uint32_t)) {
    return htobe32(value);
  }
  if constexpr (sizeof(T) == sizeof(uint16_t)) {
    return htobe16(value);
  }
  return value;
}

template <typename T> T betoh(T value) {
  if constexpr (sizeof(T) == sizeof(uint64_t)) {
    return be64toh(value);
  }
  if constexpr (sizeof(T) == sizeof(uint32_t)) {
    return be32toh(value);
  }
  if constexpr (sizeof(T) == sizeof(uint16_t)) {
    return be16toh(value);
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

  virtual const char *ReadIndex() = 0;
  virtual char *WriteIndex() = 0;

  virtual size_t Append(const char *data, size_t n) = 0;
  virtual size_t Retrieve(char *data, size_t n) = 0;
  virtual ssize_t Append(File *source) = 0;
  virtual ssize_t Retrieve(File *target) = 0;
  virtual size_t Append(Buffer *buffer) = 0;
  virtual size_t Retrieve(Buffer *buffer) = 0;

  template <typename Int> void RetrieveInt(Int *value) {
    Retrieve(reinterpret_cast<char *>(value), sizeof(Int));
    *value = betoh(*value);
  }

  template <typename Int> void AppendInt(Int value) {
    value = htobe(value);
    Append(reinterpret_cast<const char *>(&value), sizeof(Int));
  }
};

} // namespace pedrolib

#endif // PEDROLIB_BUFFER_BUFFER_H