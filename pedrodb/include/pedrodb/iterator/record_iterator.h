#ifndef PEDRODB_ITERATOR_RECORDITERATOR_H
#define PEDRODB_ITERATOR_RECORDITERATOR_H
#include "pedrodb/file/readonly_file.h"
#include "pedrodb/format/record_format.h"

namespace pedrodb {
using pedrolib::htobe;

class RecordIterator {
  ReadableFile* file_;

  record::Header header_{};

  size_t offset_{};
  size_t size_{};
  size_t buffer_offset_{};
  Buffer* buffer_;

  void FetchBuffer(size_t fetch) {
    if (buffer_->ReadableBytes() >= fetch) {
      return;
    }

    fetch = std::max((size_t)kBlockSize, fetch);
    fetch = std::min(fetch, size_ - buffer_offset_);

    buffer_->EnsureWriteable(fetch);
    file_->Read(buffer_offset_, buffer_->WriteIndex(), fetch);
    buffer_offset_ += fetch;
    buffer_->Append(fetch);
  }

 public:
  explicit RecordIterator(ReadableFile* file, Buffer* buffer)
      : file_(file), size_(file_->Size()), buffer_(buffer) {}

  bool Valid() noexcept {
    if (offset_ >= size_) {
      return false;
    }

    FetchBuffer(record::Header::SizeOf());
    if (!header_.UnPack(buffer_)) {
      PEDRODB_ERROR("file corrupt, cannot unpack header");
      return false;
    }

    if (header_.type == record::Type::kEmpty) {
      PEDRODB_ERROR("file corrupt, type is empty");
      return false;
    }

    FetchBuffer(header_.key_size + header_.value_size);
    if (buffer_->ReadableBytes() < header_.key_size + header_.value_size) {
      PEDRODB_ERROR("file corrupt");
      return false;
    }
    return true;
  }

  [[nodiscard]] uint32_t GetOffset() const noexcept { return offset_; }

  record::EntryView Next() noexcept {
    record::EntryView entry;

    entry.crc32 = header_.crc32;
    entry.type = header_.type;
    entry.timestamp = header_.timestamp;

    entry.key = {buffer_->ReadIndex(), header_.key_size};
    buffer_->Retrieve(header_.key_size);

    entry.value = {buffer_->ReadIndex(), header_.value_size};
    buffer_->Retrieve(header_.value_size);

    offset_ += entry.SizeOf();
    return entry;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_ITERATOR_RECORDITERATOR_H
