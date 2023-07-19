#ifndef PEDRODB_FORMAT_RECORD_FORMAT_H
#define PEDRODB_FORMAT_RECORD_FORMAT_H

#include <highwayhash/highwayhash_target.h>
#include <utility>

#include "pedrodb/defines.h"
#include "pedrodb/status.h"

namespace pedrodb::record {
enum class Type { kEmpty = 0, kSet = 1, kDelete = 2 };

struct Header {
  uint32_t checksum{};
  Type type{};
  uint8_t key_size{};
  uint32_t value_size{};
  uint32_t timestamp{};

  Header() = default;
  ~Header() = default;

  constexpr static size_t SizeOf() noexcept {
    return sizeof(uint8_t) +   // type
           sizeof(uint32_t) +  // checksum
           sizeof(uint8_t) +   // key_size
           sizeof(uint32_t) +  // value_size
           sizeof(uint32_t);   // timestamp
  }

  template <class Buffer>
  bool UnPack(Buffer* buffer) {
    if (buffer->ReadableBytes() < SizeOf()) {
      return false;
    }
    uint8_t u8_type;
    if (!PeekInt(buffer, &u8_type)) {
      return false;
    }
    if (u8_type == (uint8_t)Type::kEmpty) {
      return false;
    }
    RetrieveInt(buffer, &u8_type);
    RetrieveInt(buffer, &checksum);
    RetrieveInt(buffer, &key_size);
    RetrieveInt(buffer, &value_size);
    RetrieveInt(buffer, &timestamp);
    type = static_cast<Type>(u8_type);
    return true;
  }

  template <class Buffer>
  bool Pack(Buffer* buffer) const noexcept {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }
    AppendInt(buffer, (uint8_t)type);
    AppendInt(buffer, checksum);
    AppendInt(buffer, key_size);
    AppendInt(buffer, value_size);
    AppendInt(buffer, timestamp);
    return true;
  }
};

template <typename Key = std::string, typename Value = std::string>
struct Entry {
  uint32_t checksum{};
  Type type{};
  Key key{};
  Value value{};
  uint32_t timestamp{};

  [[nodiscard]] bool Validate() const noexcept {
    return checksum == Checksum(key, value);
  }

  static uint32_t Checksum(const Key& key, const Value& value) noexcept {
    using highwayhash::HHKey;
    using highwayhash::HHResult64;
    using highwayhash::HighwayHash;
    
    HHResult64 kh = 0, vh = 0;
    HighwayHash<HH_TARGET_AVX2> hash;
    HH_ALIGNAS(32) const HHKey h{0x03, 0x07, 0x32, 0xdd};
    if (std::size(key)) {
      hash(h, std::data(key), std::size(key), &kh);
    }
    if (std::size(value)) {
      hash(h, std::data(value), std::size(value), &vh);
    }
    return static_cast<uint32_t>((vh >> 32) ^ kh);
  }

  [[nodiscard]] uint32_t SizeOf() const noexcept {
    return Header::SizeOf() + std::size(key) + std::size(value);
  }

  template <class Buffer>
  bool UnPack(Buffer* buffer) {
    Header header;
    if (!header.UnPack(buffer)) {
      return false;
    }
    checksum = header.checksum;
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

  template <class Buffer>
  bool Pack(Buffer* buffer) const noexcept {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }
    Header header;
    header.checksum = checksum;
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

struct Location {
  file_id_t id{};
  uint32_t offset{};

  Location() = default;
  Location(file_id_t id, uint32_t offset) : id(id), offset(offset) {}
  ~Location() = default;

  bool operator<(const Location& other) const noexcept {
    return id != other.id ? id < other.id : offset < other.offset;
  }

  bool operator==(const Location& other) const noexcept {
    return id == other.id && offset == other.offset;
  }

  bool operator!=(const Location& other) const noexcept {
    return id != other.id || offset != other.offset;
  }

  bool operator>(const Location& other) const noexcept {
    return id != other.id ? id > other.id : offset > other.offset;
  }
};

struct Dir {
  uint32_t entry_size{};
  Location loc;
};

}  // namespace pedrodb::record

#endif  // PEDRODB_FORMAT_RECORD_FORMAT_H
