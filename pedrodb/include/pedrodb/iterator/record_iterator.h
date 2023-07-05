#ifndef PEDRODB_ITERATOR_RECORDITERATOR_H
#define PEDRODB_ITERATOR_RECORDITERATOR_H
#include "pedrodb/file/readable_file.h"
#include "pedrodb/record_format.h"

namespace pedrodb {
using pedrolib::htobe;

struct RecordView : public RecordHeader {
  std::string_view key;
  std::string_view value;

  uint32_t offset;
  uint32_t length;
};

class RecordIterator {
  ReadableFile *file_;
  RecordView view{};
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

    FetchBuffer(RecordHeader::SizeOf());
    if (!view.UnPack(buffer_)) {
      PEDRODB_ERROR("file corrupt, cannot unpack header");
      return false;
    }

    if (view.type == RecordHeader::Type::kEmpty) {
      PEDRODB_ERROR("file corrupt, type is empty");
      return false;
    }

    FetchBuffer(view.key_size + view.value_size);
    if (buffer_->ReadableBytes() < view.key_size + view.value_size) {
      PEDRODB_ERROR("file corrupt");
      return false;
    }
    return true;
  }

  RecordView Next() noexcept {
    view.key = {buffer_->ReadIndex(), view.key_size};
    buffer_->Retrieve(view.key_size);

    view.value = {buffer_->ReadIndex(), view.value_size};
    buffer_->Retrieve(view.value_size);

    view.offset = offset_;
    view.length = RecordHeader::SizeOf() + view.key_size + view.value_size;

    offset_ += view.length;
    return view;
  }
};
} // namespace pedrodb

#endif // PEDRODB_ITERATOR_RECORDITERATOR_H
