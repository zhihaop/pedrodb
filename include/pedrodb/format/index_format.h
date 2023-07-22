#ifndef PEDROKV_FORMAT_INDEX_FORMAT_H
#define PEDROKV_FORMAT_INDEX_FORMAT_H

#include "pedrodb/defines.h"
#include "pedrodb/format/record_format.h"

namespace pedrodb::index {

using ::pedrodb::record::Type;

template <typename Key>
struct Entry {
  Key key;
  Type type{};
  uint32_t offset{};
  uint32_t len{};

  [[nodiscard]] size_t SizeOf() const noexcept {
    return SizeOf(std::size(key));
  }

  [[nodiscard]] static size_t SizeOf(uint8_t key_size) noexcept {
    return sizeof(key_size) +  // key size
           sizeof(uint8_t) +   // type
           sizeof(offset) +    // file offset
           sizeof(len) +       // record entry len
           key_size;
  }

  template <class WritableBuffer>
  void Pack(WritableBuffer* buffer) const {
    buffer->EnsureWritable(SizeOf());
    AppendInt(buffer, (uint8_t)std::size(key));
    AppendInt(buffer, (uint8_t)type);
    AppendInt(buffer, offset);
    AppendInt(buffer, len);
    buffer->Append(std::data(key), std::size(key));
  }

  template <class ReadableBuffer>
  bool UnPack(ReadableBuffer* buffer) {
    uint8_t key_size;
    if (!PeekInt(buffer, &key_size)) {
      return false;
    }

    if (buffer->ReadableBytes() < SizeOf(key_size)) {
      return false;
    }

    uint8_t u8_type;
    RetrieveInt(buffer, &key_size);
    RetrieveInt(buffer, &u8_type);
    RetrieveInt(buffer, &offset);
    RetrieveInt(buffer, &len);

    type = static_cast<Type>(u8_type);
    key = Key(buffer->ReadIndex(), (size_t)key_size);
    buffer->Retrieve(key_size);
    return true;
  }
};

using EntryView = Entry<std::string_view>;

}  // namespace pedrodb::index

#endif  //PEDROKV_FORMAT_INDEX_FORMAT_H
