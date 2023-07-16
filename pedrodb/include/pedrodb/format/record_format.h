#ifndef PEDRODB_FORMAT_RECORD_FORMAT_H
#define PEDRODB_FORMAT_RECORD_FORMAT_H

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
  ~Header() = default;

  constexpr static size_t SizeOf() noexcept {
    return sizeof(uint32_t) +  // crc32
           sizeof(uint8_t) +   // type
           sizeof(uint8_t) +   // key_size
           sizeof(uint32_t) +  // value_size
           sizeof(uint32_t);   // timestamp
  }

  bool UnPack(ArrayBuffer* buffer) {
    if (buffer->ReadableBytes() < SizeOf()) {
      return false;
    }
    uint8_t u8_type;
    RetrieveInt(buffer, &crc32);
    RetrieveInt(buffer, &u8_type);
    RetrieveInt(buffer, &key_size);
    RetrieveInt(buffer, &value_size);
    RetrieveInt(buffer, &timestamp);
    type = static_cast<Type>(u8_type);
    return true;
  }

  bool Pack(ArrayBuffer* buffer) const noexcept {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }
    AppendInt(buffer, crc32);
    AppendInt(buffer, (uint8_t)type);
    AppendInt(buffer, key_size);
    AppendInt(buffer, value_size);
    AppendInt(buffer, timestamp);
    return true;
  }
};

template <typename Key, typename Value>
struct Entry {
  uint32_t crc32{};
  Type type{};
  Key key{};
  Value value{};
  uint32_t timestamp{};

  [[nodiscard]] uint32_t SizeOf() const noexcept {
    return Header::SizeOf() + std::size(key) + std::size(value);
  }

  bool UnPack(ArrayBuffer* buffer) {
    Header header;
    if (!header.UnPack(buffer)) {
      return false;
    }
    crc32 = header.crc32;
    type = header.type;
    timestamp = header.timestamp;

    if (buffer->ReadableBytes() < header.key_size + header.value_size) {
      return false;
    }

    key = Key{buffer->ReadIndex(), header.key_size};
    buffer->Retrieve(header.key_size);

    value = Value{buffer->ReadIndex(), header.value_size};
    buffer->Retrieve(header.value_size);
    return true;
  }

  bool Pack(ArrayBuffer* buffer) const noexcept {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }
    Header header;
    header.crc32 = crc32;
    header.type = type;
    header.key_size = std::size(key);
    header.value_size = std::size(value);
    header.timestamp = timestamp;

    header.Pack(buffer);
    buffer->Append(std::data(key), std::size(key));
    buffer->Append(std::data(value), std::size(value));
    return true;
  }
};

using EntryView = Entry<std::string_view, std::string_view>;

struct Location : public pedrolib::Comparable<Location> {
  struct Hash {
    size_t operator()(const Location& v) const noexcept { return v.Hash(); }
  };

  file_id_t id{};
  uint32_t offset{};

  Location() = default;
  Location(file_id_t id, uint32_t offset) : id(id), offset(offset) {}
  ~Location() = default;

  static int Compare(const Location& x, const Location& y) noexcept {
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
    size_t operator()(const Dir& other) const noexcept { return other.h; }
  };

  struct Cleaner {
    void operator()(const char* ptr) const noexcept { std::free((void*)ptr); }
  };

  using Key = std::unique_ptr<char, Cleaner>;

  uint32_t h{};
  mutable uint8_t key_size;
  mutable uint32_t entry_size;
  mutable Location loc;
  Key key;

  Dir() : key_size(0), entry_size(0) {}
  explicit Dir(uint32_t h) : h(h), key_size(0), entry_size(0) {}

  Dir(uint32_t h, std::string_view k, const Location& loc, uint32_t size)
      : h(h), loc(loc), key_size(k.size()), entry_size(size) {
    key = Key((char*)malloc(k.size()), {});
    memcpy(key.get(), k.data(), k.size());
  }

  int CompareKey(std::string_view k) const noexcept {
    const char* buf = key.get();
    for (size_t i = 0; i < key_size && i < k.size(); ++i) {
      if (buf[i] < k[i])
        return -1;
      else if (buf[i] > k[i])
        return 1;
    }
    if (key_size == k.size()) {
      return 0;
    }
    return key_size > k.size() ? 1 : -1;
  }

  ~Dir() = default;

  bool operator==(const Dir& other) const noexcept { return h == other.h; }
};
}  // namespace pedrodb::record

#endif  // PEDRODB_FORMAT_RECORD_FORMAT_H
