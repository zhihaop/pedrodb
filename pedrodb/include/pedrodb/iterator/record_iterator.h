#ifndef PEDRODB_ITERATOR_RECORDITERATOR_H
#define PEDRODB_ITERATOR_RECORDITERATOR_H
#include "pedrodb/file/random_access_file.h"
#include "pedrodb/header.h"

namespace pedrodb {
using pedrolib::htobe;

struct Record : public Header {
  std::string_view key;
  std::string_view value;

  uint32_t offset;
  uint32_t length;
};

class RecordIterator {
  std::shared_ptr<RandomAccessFile> file_;
  size_t offset_;

public:
  explicit RecordIterator(std::shared_ptr<RandomAccessFile> file)
      : file_(std::move(file)), offset_(0) {}

  bool Valid() const noexcept { return offset_ < file_->Size(); }

  Record Next() noexcept {
    Record record{};
    auto header_slice = file_->Slice(offset_, Header::PackedSize());
    Header::Unpack(&record, &header_slice);

    auto record_slice = file_->Slice(offset_ + Header::PackedSize(),
                                     record.key_size + record.value_size);

    record.key = {record_slice.Data() + record_slice.ReadIndex(),
                  record.key_size};
    record_slice.Retrieve(record.key_size);
    record.value = {record_slice.Data() + record_slice.ReadIndex(),
                    record.value_size};
    record_slice.Retrieve(record.value_size);

    record.offset = offset_;
    record.length = Header::PackedSize() + record.key_size + record.value_size;

    offset_ += record.length;
    return record;
  }
};
} // namespace pedrodb

#endif // PEDRODB_ITERATOR_RECORDITERATOR_H
