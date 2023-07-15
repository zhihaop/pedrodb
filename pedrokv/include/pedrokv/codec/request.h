#ifndef PEDROKV_CODEC_REQUEST_H
#define PEDROKV_CODEC_REQUEST_H
#include <string>
#include "pedrokv/defines.h"
#include "pedrokv/logger/logger.h"
namespace pedrokv {

enum class RequestType {
  kGet = 1,
  kPut = 2,
  kDelete = 3,
};

template <typename Key = std::string, typename Value = std::string>
struct Request {

  RequestType type;
  uint32_t id;
  Key key;
  Value value;

  [[nodiscard]] size_t SizeOf() const noexcept {
    return sizeof(uint16_t) +  // content size
           sizeof(uint8_t) +   // key size
           sizeof(uint8_t) +   // type
           sizeof(uint32_t) +  // id
           std::size(key) +    // key
           std::size(value);   // value
  }

  static uint16_t ValueSize(uint16_t content_size, uint8_t key_size) {
    return content_size -      // content size
           sizeof(uint16_t) -  // content size field
           sizeof(uint8_t) -   // key size field
           sizeof(uint8_t) -   // type field
           sizeof(id) -        // id field
           key_size;           // key size
  }

  void Pack(ArrayBuffer* buffer) const {
    if (SizeOf() > std::numeric_limits<uint16_t>::max()) {
      PEDROKV_FATAL("request size too big");
    }
    AppendInt(buffer, (uint16_t) SizeOf());
    AppendInt(buffer, (uint8_t)std::size(key));
    AppendInt(buffer, (uint8_t)type);
    AppendInt(buffer, id);
    buffer->Append(std::data(key), std::size(key));
    buffer->Append(std::data(value), std::size(value));
  }

  bool UnPack(ArrayBuffer* buffer) {

    uint8_t u8_type;
    uint8_t u8_key_size;
    uint16_t u16_value_size;
    uint16_t u16_content_size;

    if (!PeekInt(buffer, &u16_content_size)) {
      return false;
    }

    if (buffer->ReadableBytes() < u16_content_size) {
      return false;
    }

    RetrieveInt(buffer, &u16_content_size);
    RetrieveInt(buffer, &u8_key_size);
    RetrieveInt(buffer, &u8_type);
    RetrieveInt(buffer, &id);

    u16_value_size = ValueSize(u16_content_size, u8_key_size);

    type = static_cast<RequestType>(u8_type);

    key = Key{buffer->ReadIndex(), (size_t)u8_key_size};
    buffer->Retrieve(std::size(key));

    value = Value{buffer->ReadIndex(), (size_t)u16_value_size};
    buffer->Retrieve(std::size(value));

    return true;
  }

  template <typename K, typename V>
  Request& operator=(const Request<K, V>& other) {
    if (reinterpret_cast<const void*>(this) ==
        reinterpret_cast<const void*>(&other)) {
      return *this;
    }

    type = other.type;
    id = other.id;
    key = other.key;
    value = other.value;
    return *this;
  }

  template <typename K, typename V>
  bool operator==(const Request<K, V>& other) const noexcept {
    if (reinterpret_cast<const void*>(this) ==
        reinterpret_cast<const void*>(&other)) {
      return true;
    }

    return type == other.type && id == other.id && key == other.key &&
           value == other.value;
  }

  template <typename K, typename V>
  bool operator!=(const Request<K, V>& other) const noexcept {
    return !((*this) == other);
  }
};

using RequestView = Request<std::string_view, std::string_view>;

}  // namespace pedrokv
#endif  // PEDROKV_CODEC_REQUEST_H
