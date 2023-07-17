#ifndef PEDRODB_ITERATOR_INDEX_ITERATOR_H
#define PEDRODB_ITERATOR_INDEX_ITERATOR_H
#include "pedrodb/defines.h"
#include "pedrodb/format/index_format.h"
#include "pedrodb/iterator/iterator.h"

namespace pedrodb {

class IndexIterator : public Iterator<index::EntryView> {
  ArrayBuffer buffer_;
  index::EntryView entry_;

 public:
  explicit IndexIterator(ReadableFile* file) {
    buffer_.EnsureWritable(file->Size());
    file->Read(0, buffer_.WriteIndex(), buffer_.WritableBytes());
    buffer_.Append(buffer_.WritableBytes());
  }

  bool Valid() override { return entry_.UnPack(&buffer_); }

  index::EntryView Next() noexcept override { return entry_; }

  void Close() override {}
};

}  // namespace pedrodb
#endif  //PEDRODB_ITERATOR_INDEX_ITERATOR_H
