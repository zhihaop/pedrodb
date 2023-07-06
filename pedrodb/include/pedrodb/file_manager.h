#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/cache/read_cache.h"
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

  MetadataManager *metadata_manager_;

  struct OpenFile {
    file_t id{};
    std::shared_ptr<ReadableFile> file;
  };

  // std::vector is faster than std::unordered_map and std::list in this size.
  std::vector<OpenFile> open_files_;
  const uint8_t max_open_files_;

  // always in use.
  std::unique_ptr<WritableFile> active_;
  file_t active_id_{};

  ReadCache *read_cache_;

  Status OpenActiveFile(WritableFile **file, file_t id);

  Status CreateActiveFile();

public:
  FileManager(MetadataManager *metadata, ReadCache *read_cache,
              uint8_t max_open_files)
      : max_open_files_(max_open_files), metadata_manager_(metadata),
        read_cache_(read_cache) {}

  Status Init();

  auto AcquireLock() const noexcept { return std::unique_lock(mu_); }

  Status SyncActiveFile() {
    auto err = active_->Flush();
    if (!err.Empty()) {
      return Status::kIOError;
    }
    err = active_->Sync();
    if (!err.Empty()) {
      return Status::kIOError;
    }
    return Status::kOk;
  }

  Status WriteActiveFile(Buffer *buffer, file_t *id, uint32_t *offset) {
    if (active_->GetOffset() + buffer->ReadableBytes() > kMaxFileBytes) {
      auto status = CreateActiveFile();
      if (status != Status::kOk) {
        return status;
      }
      read_cache_->UpdateActiveID(active_id_);
    }

    *offset = active_->GetOffset();
    *id = active_id_;

    auto err = active_->Write(buffer);
    if (!err.Empty()) {
      return Status::kIOError;
    }
    return Status::kOk;
  }

  Status ReleaseDataFile(file_t id);

  Status AcquireDataFile(file_t id, ReadableFileGuard *file);

  Error RemoveDataFile(file_t id) {
    ReleaseDataFile(id);
    {
      auto _ = metadata_manager_->AcquireLock();
      PEDRODB_IGNORE_ERROR(metadata_manager_->DeleteFile(id));
    }
    return File::Remove(metadata_manager_->GetDataFilePath(id).c_str());
  }
};

} // namespace pedrodb
#endif // PEDRODB_FILE_MANAGER_H
