#ifndef PEDRODB_ITERATOR_INDEX_ITERATOR_H
#define PEDRODB_ITERATOR_INDEX_ITERATOR_H
#include "pedrodb/defines.h"
#include "pedrodb/format/index_format.h"

namespace pedrodb {

class IndexIterator {
  ArrayBuffer buffer_;
  index::EntryView entry_;

 public:
  explicit IndexIterator(ReadableFile* file) {
    buffer_.EnsureWritable(file->Size());
    file->Read(0, buffer_.WriteIndex(), buffer_.WritableBytes());
    buffer_.Append(buffer_.WritableBytes());
  }

  bool Valid() { return entry_.UnPack(&buffer_); }

  index::EntryView Next() noexcept { return entry_; }
};

}  // namespace pedrodb
#endif  //PEDRODB_ITERATOR_INDEX_ITERATOR_H
