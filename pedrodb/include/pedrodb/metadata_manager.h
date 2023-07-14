#ifndef PEDRODB_METADATA_MANAGER_H
#define PEDRODB_METADATA_MANAGER_H

#include "pedrodb/format/metadata_format.h"
#include "pedrodb/status.h"

#include <pedrolib/buffer/array_buffer.h>
#include <mutex>
#include <set>

namespace pedrodb {

class MetadataManager {
  mutable std::mutex mu_;

  std::string name_;
  std::set<file_t> files_;

  File file_;
  const std::string path_;

  Status Recovery();

  Status CreateDatabase();

  auto AcquireLock() const noexcept { return std::unique_lock{mu_}; }

 public:
  explicit MetadataManager(std::string path) : path_(std::move(path)) {}
  ~MetadataManager() = default;

  Status Init();

  std::vector<file_t> GetFiles() const noexcept {
    auto lock = AcquireLock();
    auto files = std::vector<file_t>{files_.begin(), files_.end()};
    lock.unlock();

    std::sort(files.begin(), files.end());
    return files;
  }

  Status CreateFile(file_t id);

  Status DeleteFile(file_t id);

  bool isActiveFile(file_t id) {
    auto lock = AcquireLock();
    if (files_.empty()) {
      return false;
    }
    return id == *files_.rbegin();
  }

  std::string GetDataFilePath(file_t id) const noexcept;
};

}  // namespace pedrodb

#endif  // PEDRODB_METADATA_MANAGER_H
