#ifndef PEDRODB_ITERATOR_RECORDITERATOR_H
#define PEDRODB_ITERATOR_RECORDITERATOR_H
#include "pedrodb/file/readonly_file.h"
#include "pedrodb/format/record_format.h"
#include "pedrodb/iterator/iterator.h"

namespace pedrodb {
using pedrolib::htobe;

class RecordIterator : public Iterator<record::EntryView> {
  ReadableFile* file_;

  record::EntryView entry_;

  size_t read_index_{};
  size_t index_{};
  const size_t size_{};

  static ArrayBuffer& GetBuffer() {
    thread_local static ArrayBuffer buffer;
    return buffer;
  }

  void fetch(size_t fetch) {
    if (read_index_ + fetch >= size_) {
      return;
    }

    auto& buffer = GetBuffer();

    buffer.Reset();
    buffer.EnsureWritable(fetch);
    file_->Read(read_index_, buffer.WriteIndex(), fetch);
    read_index_ += fetch;
    buffer.Append(fetch);
  }

 public:
  explicit RecordIterator(ReadableFile* file)
      : file_(file), size_(file_->Size()) {}

  bool Valid() noexcept override {
    if (index_ >= size_) {
      return false;
    }

    auto& buffer = GetBuffer();

    record::Header header;
    fetch(record::Header::SizeOf());
    if (!header.UnPack(&buffer)) {
      return false;
    }

    fetch(header.key_size + header.value_size);
    if (buffer.ReadableBytes() < header.key_size + header.value_size) {
      PEDRODB_ERROR("file corrupt");
      return false;
    }

    entry_.crc32 = header.crc32;
    entry_.type = header.type;
    entry_.timestamp = header.timestamp;

    entry_.key = {buffer.ReadIndex(), header.key_size};
    buffer.Retrieve(header.key_size);

    entry_.value = {buffer.ReadIndex(), header.value_size};
    buffer.Retrieve(header.value_size);
    return true;
  }

  void Seek(uint32_t offset) {
    index_ = offset;
    read_index_ = offset;
    GetBuffer().Reset();
  }

  [[nodiscard]] uint32_t GetOffset() const noexcept { return index_; }

  record::EntryView Peek() noexcept { return entry_; }
  
  record::EntryView Next() noexcept override {
    index_ += entry_.SizeOf();
    return entry_;
  }

  void Close() override {}
};
}  // namespace pedrodb

#endif  // PEDRODB_ITERATOR_RECORDITERATOR_H
