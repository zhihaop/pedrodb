#ifndef PEDROKV_CODEC_RESPONSE_H
#define PEDROKV_CODEC_RESPONSE_H

namespace pedrokv {

enum class ResponseType { kOk, kError };

template <typename Value = std::string>
struct Response {
  ResponseType type{};
  uint32_t id{};
  Value data;

  [[nodiscard]] uint16_t SizeOf() const noexcept {
    return sizeof(uint16_t) +  // content size
           sizeof(uint8_t) +   // type
           sizeof(uint32_t) +  // id
           std::size(data);    // data
  }

  static uint16_t DataSize(uint16_t content_size) {
    return content_size -      // content size
           sizeof(uint16_t) -  // content size field
           sizeof(uint8_t) -   // type field
           sizeof(uint32_t);   // id field
  }

  void Pack(ArrayBuffer* buffer) const {
    AppendInt(buffer, SizeOf());
    AppendInt(buffer, (uint8_t)type);
    AppendInt(buffer, id);
    buffer->Append(std::data(data), std::size(data));
  }

  bool UnPack(ArrayBuffer* buffer) {
    uint8_t u8_type;
    uint16_t u16_content_size;

    if (!PeekInt(buffer, &u16_content_size)) {
      return false;
    }
    
    if (buffer->ReadableBytes() < u16_content_size) {
      return false;
    }

    RetrieveInt(buffer, &u16_content_size);
    RetrieveInt(buffer, &u8_type);
    RetrieveInt(buffer, &id);

    type = static_cast<ResponseType>(u8_type);
    data = Value{buffer->ReadIndex(), (size_t)DataSize(u16_content_size)};
    buffer->Retrieve(std::size(data));
    return true;
  }

  template <typename V>
  Response& operator=(const Response<V>& other) {
    if (reinterpret_cast<const void*>(this) ==
        reinterpret_cast<const void*>(&other)) {
      return *this;
    }

    type = other.type;
    id = other.id;
    data = other.data;
    return *this;
  }
};

using ResponseView = Response<std::string_view>;

}  // namespace pedrokv
#endif  // PEDROKV_CODEC_RESPONSE_H
