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
  size_t offset_;
  ArrayBuffer buffer_;

public:
  explicit RecordIterator(ReadableFile *file) : file_(file), offset_(0) {}

  bool Valid() noexcept {
    if (offset_ + RecordHeader::SizeOf() >= file_->Size()) {
      return false;
    }

    buffer_.Reset();
    buffer_.EnsureWriteable(RecordHeader::SizeOf());
    file_->Read(offset_, buffer_.Data() + buffer_.WriteIndex(),
                RecordHeader::SizeOf());
    buffer_.Append(RecordHeader::SizeOf());

    RecordHeader::Unpack(&view, &buffer_);
    return view.type != RecordHeader::kEmpty;
  }

  RecordView Next() noexcept {
    buffer_.Reset();
    buffer_.EnsureWriteable(view.key_size + view.value_size);
    file_->Read(offset_ + RecordHeader::SizeOf(),
                buffer_.Data() + buffer_.WriteIndex(),
                view.key_size + view.value_size);
    buffer_.Append(view.key_size + view.value_size);

    view.key = {buffer_.Data() + buffer_.ReadIndex(), view.key_size};

    buffer_.Retrieve(view.key_size);
    view.value = {buffer_.Data() + buffer_.ReadIndex(), view.value_size};
    buffer_.Retrieve(view.value_size);

    view.offset = offset_;
    view.length = RecordHeader::SizeOf() + view.key_size + view.value_size;

    offset_ += view.length;
    return view;
  }
};
} // namespace pedrodb

#endif // PEDRODB_ITERATOR_RECORDITERATOR_H
