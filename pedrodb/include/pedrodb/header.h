#ifndef PEDRODB_HEADER_H
#define PEDRODB_HEADER_H

#include "pedrodb/defines.h"

namespace pedrodb {
struct Header {
  enum HeaderType { kSet = 0, kDelete = 1 };

  uint32_t crc32;
  uint16_t type;
  uint16_t key_size;
  uint32_t value_size;
  uint64_t timestamp;

  constexpr static size_t PackedSize() noexcept {
    return sizeof(crc32) + sizeof(type) + sizeof(key_size) +
           sizeof(value_size) + sizeof(timestamp);
  }

  static bool Unpack(Header *header, Buffer *buffer) {
    if (buffer->ReadableBytes() < PackedSize()) {
      return false;
    }
    buffer->RetrieveInt(&header->crc32);
    buffer->RetrieveInt(&header->type);
    buffer->RetrieveInt(&header->key_size);
    buffer->RetrieveInt(&header->value_size);
    buffer->RetrieveInt(&header->timestamp);
    return true;
  }

  bool Pack(Buffer *buffer) const noexcept {
    if (buffer->WritableBytes() < PackedSize()) {
      return false;
    }
    buffer->AppendInt(crc32);
    buffer->AppendInt(type);
    buffer->AppendInt(key_size);
    buffer->AppendInt(value_size);
    buffer->AppendInt(timestamp);
    return true;
  }
};
} // namespace pedrodb

#endif // PEDRODB_HEADER_H
