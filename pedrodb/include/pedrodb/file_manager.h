#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"

#include <pedrolib/collection/lru_unordered_map.h>

namespace pedrodb {

using ReadableFileGuard = std::shared_ptr<ReadableFile>;

class FileManager {
  mutable std::mutex mu_;

  MetadataManager *metadata_;

  struct OpenFile {
    uint32_t id{};
    std::shared_ptr<ReadableFile> file;
  };

  // std::vector is faster than std::unordered_map and std::list in this size.
  std::vector<OpenFile> open_files_;
  const uint8_t max_open_files_;

  // always in use.
  std::unique_ptr<WritableFile> active_;
  uint32_t active_id_{};

  Status OpenActiveFile(WritableFile **file, uint32_t id);

public:
  FileManager(MetadataManager *metadata, uint8_t max_open_files)
      : max_open_files_(max_open_files), metadata_(metadata) {}

  Status Init();

  auto AcquireLock() const noexcept { return std::unique_lock(mu_); }

  Status GetActiveFile(WritableFile **file, uint32_t *id) noexcept {
    *file = active_.get();
    *id = active_id_;
    return Status::kOk;
  }

  Status CreateActiveFile(WritableFile **file, uint32_t *id);

  uint32_t GetActiveFileID() const noexcept { return active_id_; }

  Status ReleaseDataFile(uint32_t id);

  Status AcquireDataFile(uint32_t id, ReadableFileGuard *file);

  Error RemoveDataFile(uint32_t id) {
    ReleaseDataFile(id);
    {
      auto _ = metadata_->AcquireLock();
      PEDRODB_IGNORE_ERROR(metadata_->DeleteFile(id));
    }
    return File::Remove(metadata_->GetDataFilePath(id).c_str());
  }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_MANAGER_H
