#ifndef PEDRODB_METADATA_MANAGER_H
#define PEDRODB_METADATA_MANAGER_H

#include "pedrodb/format/metadata_format.h"
#include "pedrodb/status.h"

#include <pedrolib/buffer/array_buffer.h>
#include <mutex>
#include <set>

namespace pedrodb {

class MetadataManager : public std::enable_shared_from_this<MetadataManager> {
  mutable std::mutex mu_;

  std::string name_;
  std::set<file_id_t> files_;

  File file_;
  const std::string path_;

  Status Recovery();

  Status CreateDatabase();

  auto AcquireLock() const noexcept { return std::unique_lock{mu_}; }

 public:
  using Ptr = std::shared_ptr<MetadataManager>;

  explicit MetadataManager(std::string path) : path_(std::move(path)) {}
  ~MetadataManager() = default;

  Status Init();

  std::vector<file_id_t> GetFiles() const noexcept {
    auto lock = AcquireLock();
    return {files_.begin(), files_.end()};
  }

  Status CreateFile(file_id_t id);

  Status DeleteFile(file_id_t id);

  std::string GetDataFilePath(file_id_t id) const noexcept;

  std::string GetIndexFilePath(file_id_t id) const noexcept;
};

}  // namespace pedrodb

#endif  // PEDRODB_METADATA_MANAGER_H
