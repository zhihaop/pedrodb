#ifndef PEDROLIB_BUFFER_BUFFER_H
#define PEDROLIB_BUFFER_BUFFER_H

#include <cstdio>
#include <cstring>
#include <string_view>
#include "pedrolib/file/file.h"

namespace pedrolib {

template <typename T>
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

template <typename T>
T betoh(T value) {
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

template <typename Buffer, typename Int>
bool RetrieveInt(Buffer* buffer, Int* value) {
  if (buffer->ReadableBytes() < sizeof(Int)) {
    return false;
  }
  *value = betoh(*reinterpret_cast<const Int*>(buffer->ReadIndex()));
  buffer->Retrieve(sizeof(Int));
  return true;
}

template <typename Buffer, typename Int>
bool PeekInt(Buffer* buffer, Int* value) {
  if (buffer->ReadableBytes() < sizeof(Int)) {
    return false;
  }
  *value = betoh(*reinterpret_cast<const Int*>(buffer->ReadIndex()));
  return true;
}

template <typename Buffer, typename Int>
void AppendInt(Buffer* buffer, Int value) {
  value = htobe(value);
  buffer->Append(reinterpret_cast<const char*>(&value), sizeof(Int));
}

}  // namespace pedrolib

#endif  // PEDROLIB_BUFFER_BUFFER_H