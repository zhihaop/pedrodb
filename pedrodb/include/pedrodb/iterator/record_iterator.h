#ifndef PEDRODB_ITERATOR_RECORDITERATOR_H
#define PEDRODB_ITERATOR_RECORDITERATOR_H
#include "pedrodb/file/readonly_file.h"
#include "pedrodb/format/record_format.h"

namespace pedrodb {
using pedrolib::htobe;

class RecordIterator {
  ReadableFile* file_;

  record::Header header_{};

  size_t read_index_{};
  const size_t size_{};
  size_t write_index_{};
  ArrayBuffer buffer_;

  void FetchBuffer(size_t fetch) {
    if (buffer_.ReadableBytes() >= fetch) {
      return;
    }

    fetch = std::max((size_t)kPageSize, fetch);
    fetch = std::min(fetch, size_ - write_index_);

    buffer_.EnsureWritable(fetch);
    file_->Read(write_index_, buffer_.WriteIndex(), fetch);
    write_index_ += fetch;
    buffer_.Append(fetch);
  }

 public:
  explicit RecordIterator(ReadableFile* file)
      : file_(file), size_(file_->Size()) {}

  bool Valid() noexcept {
    if (read_index_ >= size_) {
      return false;
    }

    FetchBuffer(record::Header::SizeOf());
    if (!header_.UnPack(&buffer_)) {
      return false;
    }

    FetchBuffer(header_.key_size + header_.value_size);
    if (buffer_.ReadableBytes() < header_.key_size + header_.value_size) {
      PEDRODB_ERROR("file corrupt");
      return false;
    }
    return true;
  }

  [[nodiscard]] uint32_t GetOffset() const noexcept { return read_index_; }

  record::EntryView Next() noexcept {
    record::EntryView entry;

    entry.crc32 = header_.crc32;
    entry.type = header_.type;
    entry.timestamp = header_.timestamp;

    entry.key = {buffer_.ReadIndex(), header_.key_size};
    buffer_.Retrieve(header_.key_size);

    entry.value = {buffer_.ReadIndex(), header_.value_size};
    buffer_.Retrieve(header_.value_size);

    read_index_ += entry.SizeOf();
    return entry;
  }
};
}  // namespace pedrodb

#endif  // PEDRODB_ITERATOR_RECORDITERATOR_H
