#ifndef PEDROKV_CODEC_REQUEST_H
#define PEDROKV_CODEC_REQUEST_H
#include "pedrokv/defines.h"
#include <string>
namespace pedrokv {

struct Request {
  enum Type {
    kGet,
    kSet,
    kDelete,
  };

  Type type;
  uint32_t id;
  std::string key;
  std::string value;

  [[nodiscard]] uint16_t SizeOf() const noexcept { return SizeOf(key, value); }

  void Pack(Buffer *buffer) const { Pack(type, id, key, value, buffer); }

  static uint16_t SizeOf(std::string_view key,
                         std::string_view value) noexcept {
    return sizeof(uint8_t) +  // type
           sizeof(uint32_t) + // id
           sizeof(uint16_t) + // key size
           sizeof(uint16_t) + // value size
           key.size() +       // key
           value.size();      // value
  }

  static void Pack(Type type, uint32_t id, std::string_view key,
                   std::string_view value, Buffer *buffer) {
    auto u8_type = static_cast<uint8_t>(type);
    uint16_t key_size = key.size();
    uint16_t value_size = value.size();

    buffer->AppendInt(u8_type);
    buffer->AppendInt(id);
    buffer->AppendInt(key_size);
    buffer->AppendInt(value_size);
    buffer->Append(key.data(), key.size());
    buffer->Append(value.data(), value.size());
  }

  void UnPack(Buffer *buffer) {
    uint8_t u8_type;
    uint16_t key_size;
    uint16_t value_size;

    buffer->RetrieveInt(&u8_type);
    buffer->RetrieveInt(&id);
    buffer->RetrieveInt(&key_size);
    buffer->RetrieveInt(&value_size);

    type = static_cast<Type>(u8_type);
    key.resize(key_size);
    value.resize(value_size);

    buffer->Retrieve(key.data(), key.size());
    buffer->Retrieve(value.data(), value.size());
  }
};

} // namespace pedrokv
#endif // PEDROKV_CODEC_REQUEST_H
