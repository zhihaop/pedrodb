#ifndef PEDRODB_RECORD_FORMAT_H
#define PEDRODB_RECORD_FORMAT_H

#include "pedrodb/defines.h"
#include "pedrodb/status.h"

namespace pedrodb {
struct RecordHeader {
  enum class Type { kEmpty = 0, kSet = 1, kDelete = 2 };

  uint32_t crc32;
  Type type;
  uint16_t key_size;
  uint32_t value_size;
  uint32_t timestamp;

  constexpr static size_t SizeOf() noexcept {
    return sizeof(crc32) + sizeof(uint16_t) + sizeof(key_size) +
           sizeof(value_size) + sizeof(timestamp);
  }

  bool UnPack(Buffer *buffer) {
    if (buffer->ReadableBytes() < SizeOf()) {
      return false;
    }
    uint16_t u16type;
    buffer->RetrieveInt(&crc32);
    buffer->RetrieveInt(&u16type);
    buffer->RetrieveInt(&key_size);
    buffer->RetrieveInt(&value_size);
    buffer->RetrieveInt(&timestamp);
    
    type = static_cast<Type>(u16type);
    return true;
  }

  bool Pack(Buffer *buffer) const noexcept {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }
    buffer->AppendInt(crc32);
    buffer->AppendInt(static_cast<uint16_t>(type));
    buffer->AppendInt(key_size);
    buffer->AppendInt(value_size);
    buffer->AppendInt(timestamp);
    return true;
  }
};
} // namespace pedrodb

#endif // PEDRODB_RECORD_FORMAT_H
