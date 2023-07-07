#ifndef PEDRODB_FILE_MANAGER_H
#define PEDRODB_FILE_MANAGER_H

#include "pedrodb/cache/read_cache.h"
#include "pedrodb/defines.h"
#include "pedrodb/file/readable_file_impl.h"
#include "pedrodb/file/writable_file.h"
#include "pedrodb/logger/logger.h"
#include "pedrodb/metadata_manager.h"

#include <pedrolib/collection/lru_unordered_map.h>

namespace pedrodb {

using ReadableFileGuard = std::shared_ptr<ReadableFile>;
using WritableFileGuard = std::unique_ptr<WritableFile>;

class FileManager;

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
  std::string memtable_;
  record::Location latest_;

  Status OpenActiveFile(WritableFileGuard *file, file_t id);

  Status CreateActiveFile();

  Status Recovery(file_t active);

public:
  FileManager(MetadataManager *metadata, uint8_t max_open_files)
      : max_open_files_(max_open_files), metadata_manager_(metadata) {
    memtable_.reserve(kMaxFileBytes);
  }

  Status Init();

  auto AcquireLock() const noexcept { return std::unique_lock(mu_); }

  Status Sync();

  Status Flush(bool force);

  Status WriteActiveFile(Buffer *buffer, record::Location *loc);

  void ReleaseDataFile(file_t id);

  file_t GetActiveFile() const noexcept { return latest_.id; }

  Status AcquireDataFile(file_t id, ReadableFileGuard *file);

  Error RemoveDataFile(file_t id);
};

class MemoryTableView : public ReadableFile, nonmovable {
  mutable ReadableFileGuard file_;
  mutable const std::string *memtable_;
  FileManager *parent_{};
  file_t id_;

public:
  MemoryTableView(const std::string *memtable, FileManager *parent)
      : memtable_(memtable), parent_(parent), id_(parent->GetActiveFile()) {}

  ~MemoryTableView() override = default;

  void UpdateFile() const noexcept;

  uint64_t Size() const noexcept override;

  Error GetError() const noexcept override;

  ssize_t Read(uint64_t offset, char *buf, size_t n) override;
};

} // namespace pedrodb
#endif // PEDRODB_FILE_MANAGER_H
