#ifndef PEDRODB_METADATA_FORMAT_H
#define PEDRODB_METADATA_FORMAT_H

#include <pedrolib/buffer/buffer.h>

namespace pedrodb::metadata {
using pedrolib::htobe;

struct Header {
  std::string name;

  static size_t SizeOf(size_t name_length) {
    return sizeof(uint32_t) + name_length;
  }

  bool UnPack(Buffer *buffer) {
    if (buffer->ReadableBytes() < SizeOf(0)) {
      return false;
    }

    uint32_t length;
    buffer->RetrieveInt(&length);

    if (buffer->ReadableBytes() < length) {
      return false;
    }

    name.resize(length);
    buffer->Retrieve(name.data(), name.size());
    return true;
  }

  bool Pack(Buffer *buffer) const {
    buffer->AppendInt(static_cast<uint32_t>(name.size()));
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

  static size_t SizeOf() noexcept { return sizeof(uint8_t) + sizeof(file_t); }

  bool UnPack(Buffer *buffer) {
    if (buffer->ReadableBytes() < SizeOf()) {
      return false;
    }

    uint8_t u8_type;
    buffer->RetrieveInt(&u8_type);
    buffer->RetrieveInt(&id);
    type = static_cast<LogType>(u8_type);
    return true;
  }

  bool Pack(Buffer *buffer) const {
    if (buffer->WritableBytes() < SizeOf()) {
      return false;
    }

    buffer->AppendInt(static_cast<uint8_t>(type));
    buffer->AppendInt(id);
    return true;
  }
};
} // namespace pedrodb::metadata

#endif // PEDRODB_METADATA_FORMAT_H
