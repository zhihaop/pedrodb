#ifndef PEDRODB_FORMAT_METADATA_FORMAT_H
#define PEDRODB_FORMAT_METADATA_FORMAT_H

#include "pedrodb/defines.h"

namespace pedrodb::metadata {
using pedrolib::htobe;

struct Header {
  std::string name;

  static size_t SizeOf(size_t name_length) {
    return sizeof(uint32_t) + name_length;
  }

  bool UnPack(ArrayBuffer* buffer) {
    if (buffer->ReadableBytes() < SizeOf(0)) {
      return false;
    }

    uint32_t length;
    RetrieveInt(buffer, &length);

    if (buffer->ReadableBytes() < length) {
      return false;
    }

    name.resize(length);
    buffer->Retrieve(name.data(), name.size());
    return true;
  }

  bool Pack(ArrayBuffer* buffer) const {
    AppendInt(buffer, static_cast<uint32_t>(name.size()));
    buffer->Append(name.data(), name.size());
    return true;
  }
};

enum class LogType {
  kCreateFile,
  kDeleteFile,
};

struct LogEntry {
  LogType type{};
  file_t id{};
  LogEntry() = default;
  LogEntry(LogType type, file_t id) : type(type), id(id) {}
  ~LogEntry() = default;

  static size_t SizeOf() noexcept { return sizeof(uint8_t) + sizeof(file_t); }

  bool UnPack(ArrayBuffer* buffer) {
    if (buffer->ReadableBytes() < SizeOf()) {
      return false;
    }

    uint8_t u8_type;
    RetrieveInt(buffer, &u8_type);
    RetrieveInt(buffer, &id);
    type = static_cast<LogType>(u8_type);
    return true;
  }

  bool Pack(ArrayBuffer* buffer) const {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }

    AppendInt(buffer, static_cast<uint8_t>(type));
    AppendInt(buffer, id);
    return true;
  }
};
}  // namespace pedrodb::metadata

#endif  // PEDRODB_FORMAT_METADATA_FORMAT_H
