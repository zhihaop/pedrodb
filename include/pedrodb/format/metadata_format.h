#ifndef PEDRODB_FORMAT_METADATA_FORMAT_H
#define PEDRODB_FORMAT_METADATA_FORMAT_H

#include "pedrodb/defines.h"

namespace pedrodb::metadata {
using pedrolib::htobe;

struct Header {
  std::string name;

  static size_t SizeOf(size_t name_length) {
    return sizeof(uint16_t) + name_length;
  }

  bool UnPack(ArrayBuffer* buffer) {
    if (buffer->ReadableBytes() < SizeOf(0)) {
      return false;
    }

    uint16_t length;
    RetrieveInt(buffer, &length);

    if (buffer->ReadableBytes() < length) {
      return false;
    }

    name.resize(length);
    buffer->Retrieve(name.data(), name.size());
    return true;
  }

  void Pack(ArrayBuffer* buffer) const {
    AppendInt(buffer, (uint16_t)(name.size()));
    buffer->Append(name.data(), name.size());
  }
};

enum class LogType {
  kCreateFile,
  kDeleteFile,
};

struct LogEntry {
  LogType type{};
  file_id_t id{};
  LogEntry() = default;
  ~LogEntry() = default;

  static size_t SizeOf() noexcept {
    return sizeof(uint8_t) + sizeof(file_id_t);
  }
  
  template <class ReadableBuffer>
  bool UnPack(ReadableBuffer* buffer) {
    if (buffer->ReadableBytes() < SizeOf()) {
      return false;
    }

    uint8_t u8_type;
    RetrieveInt(buffer, &u8_type);
    RetrieveInt(buffer, &id);
    type = static_cast<LogType>(u8_type);
    return true;
  }
  
  template <class WritableBuffer>
  void Pack(WritableBuffer* buffer) const {
    AppendInt(buffer, (uint8_t)type);
    AppendInt(buffer, id);
  }
};
}  // namespace pedrodb::metadata

#endif  // PEDRODB_FORMAT_METADATA_FORMAT_H
