#ifndef PEDRODB_FILE_WRITABLE_FILE_H
#define PEDRODB_FILE_WRITABLE_FILE_H
#include "pedrodb/defines.h"

namespace pedrodb {

struct WritableFile {
  using Ptr = std::shared_ptr<WritableFile>;

  WritableFile() = default;
  virtual ~WritableFile() = default;

  virtual Error Flush(bool force) = 0;
  virtual Error Sync() = 0;
};
}  // namespace pedrodb

#endif  //PEDRODB_FILE_WRITABLE_FILE_H
