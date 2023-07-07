#ifndef PEDRODB_RECORD_FORMAT_H
#define PEDRODB_RECORD_FORMAT_H

#include "pedrodb/defines.h"
#include "pedrodb/status.h"

namespace pedrodb::record {
enum class Type { kEmpty = 0, kSet = 1, kDelete = 2 };

struct Header {
  uint32_t crc32;
  Type type;
  uint8_t key_size;
  uint32_t value_size;
  uint32_t timestamp;

  constexpr static size_t SizeOf() noexcept {
    return sizeof(uint32_t) + // crc32
           sizeof(uint8_t) +  // type
           sizeof(uint8_t) +  // key_size
           sizeof(uint32_t) + // value_size
           sizeof(uint32_t);  // timestamp
  }

  bool UnPack(Buffer *buffer) {
    if (buffer->ReadableBytes() < SizeOf()) {
      return false;
    }
    uint8_t u8_type;
    buffer->RetrieveInt(&crc32);
    buffer->RetrieveInt(&u8_type);
    buffer->RetrieveInt(&key_size);
    buffer->RetrieveInt(&value_size);
    buffer->RetrieveInt(&timestamp);
    type = static_cast<Type>(u8_type);
    return true;
  }

  bool Pack(Buffer *buffer) const noexcept {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }
    buffer->AppendInt(crc32);
    buffer->AppendInt((uint8_t)type);
    buffer->AppendInt(key_size);
    buffer->AppendInt(value_size);
    buffer->AppendInt(timestamp);
    return true;
  }
};

constexpr static size_t SizeOf(uint8_t key_size, uint32_t value_size) {
  return Header::SizeOf() + key_size + value_size;
}

struct Location : public pedrolib::Comparable<Location> {
  struct Hash {
    size_t operator()(const Location &v) const noexcept { return v.Hash(); }
  };

  file_t id{};
  uint32_t offset{};

  static int Compare(const Location &x, const Location &y) noexcept {
    if (x.id != y.id) {
      return x.id < y.id ? -1 : 1;
    }
    if (x.offset != y.offset) {
      return x.offset < y.offset ? -1 : 1;
    }
    return 0;
  }

  [[nodiscard]] size_t Hash() const noexcept {
    return (((uint64_t)id) << 32) | offset;
  }
};
} // namespace pedrodb::record

#endif // PEDRODB_RECORD_FORMAT_H
