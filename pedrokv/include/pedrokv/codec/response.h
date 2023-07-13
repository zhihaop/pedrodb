#ifndef PEDROKV_CODEC_RESPONSE_H
#define PEDROKV_CODEC_RESPONSE_H

namespace pedrokv {

struct Response {
  enum Type { kOk, kError };

  Type type;
  uint32_t id;
  std::string data;

  [[nodiscard]] uint16_t SizeOf() const noexcept {
    return sizeof(uint8_t) +  // type
           sizeof(uint32_t) + // id
           sizeof(uint16_t) + // data size
           data.size();       // data
  }

  void Pack(Buffer *buffer) const {
    auto u8_type = static_cast<uint8_t>(type);
    uint16_t size = data.size();

    buffer->AppendInt(u8_type);
    buffer->AppendInt(id);
    buffer->AppendInt(size);
    buffer->Append(data.data(), data.size());
  }

  void UnPack(Buffer *buffer) {
    uint8_t u8_type;
    uint16_t size;

    buffer->RetrieveInt(&u8_type);
    buffer->RetrieveInt(&id);
    buffer->RetrieveInt(&size);

    type = static_cast<Type>(u8_type);
    data.resize(size);
    buffer->Retrieve(data.data(), data.size());
  }
};

} // namespace pedrokv
#endif // PEDROKV_CODEC_RESPONSE_H
