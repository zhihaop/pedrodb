#ifndef PEDRODB_ITERATOR_RECORDITERATOR_H
#define PEDRODB_ITERATOR_RECORDITERATOR_H
#include "pedrodb/file/readable_file.h"
#include "pedrodb/header.h"

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

  void FetchBuffer(size_t size) {
    if (buffer_->ReadableBytes() >= size) {
      return;
    }

    if (buffer_offset_ >= size_) {
      return;
    }

    size_t next = buffer_offset_ + size;
    next = std::min(next - (next % kPageSize) + kPageSize, size_);

    size_t r = (next - buffer_offset_);
    buffer_->EnsureWriteable(r);
    file_->Read(buffer_offset_, buffer_->WriteIndex(), r);
    buffer_offset_ = next;
    buffer_->Append(r);
  }

public:
  explicit RecordIterator(ReadableFile *file, Buffer *buffer)
      : file_(file), size_(file_->Size()), buffer_(buffer) {}

  explicit RecordIterator(ReadableFile *file, size_t size, Buffer *buffer)
      : file_(file), size_(size), buffer_(buffer) {}

  void SetOffset(size_t offset) {
    buffer_->Reset();
    buffer_offset_ = offset_ = offset;
  }

  bool Valid() noexcept {
    FetchBuffer(RecordHeader::SizeOf());
    if (buffer_->ReadableBytes() < RecordHeader::SizeOf()) {
      return false;
    }

    RecordHeader::Unpack(&view, buffer_);
    if (view.type == RecordHeader::kEmpty) {
      return false;
    }

    FetchBuffer(view.key_size + view.value_size);
    if (buffer_->ReadableBytes() < view.key_size + view.value_size) {
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
