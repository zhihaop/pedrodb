#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/cache/read_cache.h"
#include "pedrodb/defines.h"
#include "pedrodb/file/readonly_file.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"

namespace pedrodb {

using ReadableFileGuard = std::shared_ptr<ReadableFile>;
using WritableFileGuard = std::shared_ptr<WritableFile>;

class FileManager;

class FileManager {
  mutable std::mutex mu_;

  MetadataManager* metadata_manager_;

  struct OpenFile {
    file_t id{};
    std::shared_ptr<ReadableFile> file;
  };

  // std::vector is faster than std::unordered_map and std::list in this size.
  std::vector<OpenFile> open_files_;
  const uint8_t max_open_files_;

  // always in use.
  std::shared_ptr<WritableFile> active_;

  Status OpenActiveFile(WritableFileGuard* file, file_t id);

  Status CreateActiveFile();

  Status Recovery(file_t active);

  auto AcquireLock() const noexcept { return std::unique_lock(mu_); }

 public:
  FileManager(MetadataManager* metadata, uint8_t max_open_files)
      : max_open_files_(max_open_files), metadata_manager_(metadata) {}

  Status Init();

  Status Sync();

  Status Flush(bool force);

  template <typename Key, typename Value>
  Status WriteActiveFile(const record::Entry<Key, Value>& entry,
                         record::Location* loc) {
    for (auto lock = AcquireLock();;) {
      size_t offset = active_->Write(entry);
      if (offset != -1) {
        loc->offset = offset;
        loc->id = active_->GetFile();
        return Status::kOk;
      }

      auto status = CreateActiveFile();
      if (status != Status::kOk) {
        return status;
      }
    }
  }

  void ReleaseDataFile(file_t id);

  Status AcquireDataFile(file_t id, ReadableFileGuard* file);

  Error RemoveDataFile(file_t id);
};
}  // namespace pedrodb
#endif  // PEDRODB_FILE_MANAGER_H
