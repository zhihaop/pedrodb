#ifndef PEDRODB_ITERATOR_RECORDITERATOR_H
#define PEDRODB_ITERATOR_RECORDITERATOR_H
#include "pedrodb/file/readable_file.h"
#include "pedrodb/record_format.h"

namespace pedrodb {
using pedrolib::htobe;

struct RecordEntry {
  std::string_view key;
  std::string_view value;

  record::Type type{};
  uint32_t timestamp{};
  uint32_t offset{};
};

class RecordIterator {
  ReadableFile *file_;

  record::Header view_{};

  size_t offset_{};
  size_t size_{};
  size_t buffer_offset_{};
  Buffer *buffer_;

  void FetchBuffer(size_t fetch) {
    if (buffer_->ReadableBytes() >= fetch) {
      return;
    }

    fetch = std::max((size_t)kBlockSize, fetch);
    fetch = std::min(fetch, size_ - buffer_offset_);

    PEDRODB_TRACE("fetch {}", fetch);
    buffer_->EnsureWriteable(fetch);
    file_->Read(buffer_offset_, buffer_->WriteIndex(), fetch);
    buffer_offset_ += fetch;
    buffer_->Append(fetch);
  }

public:
  explicit RecordIterator(ReadableFile *file, Buffer *buffer)
      : file_(file), size_(file_->Size()), buffer_(buffer) {}

  bool Valid() noexcept {
    if (offset_ >= size_) {
      return false;
    }

    FetchBuffer(record::Header::SizeOf());
    if (!view_.UnPack(buffer_)) {
      PEDRODB_ERROR("file corrupt, cannot unpack header");
      return false;
    }

    if (view_.type == record::Type::kEmpty) {
      PEDRODB_ERROR("file corrupt, type is empty");
      return false;
    }

    FetchBuffer(view_.key_size + view_.value_size);
    if (buffer_->ReadableBytes() < view_.key_size + view_.value_size) {
      PEDRODB_ERROR("file corrupt");
      return false;
    }
    return true;
  }

  RecordEntry Next() noexcept {
    RecordEntry entry;
    entry.key = {buffer_->ReadIndex(), view_.key_size};
    buffer_->Retrieve(view_.key_size);

    entry.value = {buffer_->ReadIndex(), view_.value_size};
    buffer_->Retrieve(view_.value_size);

    entry.offset = offset_;
    entry.type = view_.type;
    entry.timestamp = view_.timestamp;
    offset_ += record::SizeOf(view_.key_size, view_.value_size);
    return entry;
  }
};
} // namespace pedrodb

#endif // PEDRODB_ITERATOR_RECORDITERATOR_H
