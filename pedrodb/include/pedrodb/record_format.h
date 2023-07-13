#ifndef PEDRODB_RECORD_FORMAT_H
#define PEDRODB_RECORD_FORMAT_H

#include <utility>

#include "pedrodb/defines.h"
#include "pedrodb/status.h"

namespace pedrodb::record {
enum class Type { kEmpty = 0, kSet = 1, kDelete = 2 };

struct Header {
  uint32_t crc32{};
  Type type{};
  uint8_t key_size{};
  uint32_t value_size{};
  uint32_t timestamp{};

  Header() = default;
  Header(uint32_t crc32, Type type, uint8_t keySize, uint32_t valueSize,
         uint32_t timestamp)
      : crc32(crc32), type(type), key_size(keySize), value_size(valueSize),
        timestamp(timestamp) {}

  ~Header() = default;

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

  Location() = default;
  Location(file_t id, uint32_t offset) : id(id), offset(offset) {}
  ~Location() = default;

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

struct Dir {
  struct Hash {
    size_t operator()(const Dir &other) const noexcept { return other.h; }
  };

  struct Cleaner {
    void operator()(const char *ptr) const noexcept { std::free((void *)ptr); }
  };

  using Key = std::unique_ptr<char, Cleaner>;

  uint32_t h{};
  Key key;
  mutable Location loc;
  mutable uint32_t size{};

  Dir() = default;
  explicit Dir(uint32_t h) : h(h) {}
  Dir(uint32_t h, std::string_view k, const Location &loc, uint32_t size)
      : h(h), loc(loc), size(size) {
    key = Key((char *)malloc(k.size() + 1), Cleaner{});
    key.get()[k.size()] = 0;
    memcpy(key.get(), k.data(), k.size());
  }

  int CompareKey(std::string_view k) const noexcept {
    size_t i;
    const char *buf = key.get();
    for (i = 0; buf[i] && i < k.size(); ++i) {
      if (buf[i] < k[i])
        return -1;
      else if (buf[i] > k[i])
        return 1;
    }
    if (i == k.size() && buf[i] == 0) {
      return 0;
    }
    return i == k.size() ? 1 : -1;
  }

  ~Dir() = default;

  bool operator==(const Dir &other) const noexcept { return h == other.h; }
};
} // namespace pedrodb::record

#endif // PEDRODB_RECORD_FORMAT_H
