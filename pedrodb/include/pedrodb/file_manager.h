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

class MemoryTable : nonmovable, public ReadableFile {
  std::string buffer_;
  std::atomic<file_t> id_{};

public:
  explicit MemoryTable(size_t capacity) { buffer_.resize(capacity); }
  ~MemoryTable() override = default;

  [[nodiscard]] uint64_t Size() const noexcept override {
    return buffer_.size();
  }

  [[nodiscard]] Error GetError() const noexcept override {
    return Error::Success();
  }

  ssize_t Read(uint64_t offset, char *buf, size_t n) override {
    size_t sentry = offset + n;
    sentry = std::min(sentry, buffer_.size());

    size_t r = sentry - offset;
    std::copy_n(buffer_.data() + offset, r, buf);
    return static_cast<ssize_t>(r);
  }

  [[nodiscard]] const char *Data() const noexcept { return buffer_.data(); }

  void Update(file_t id) {
    id_ = id;
    buffer_.clear();
  }

  void Update(file_t id, std::string value) {
    id_ = id;
    buffer_ = std::move(value);
  }

  [[nodiscard]] file_t GetID() const noexcept { return id_; }
  void Append(const char *buf, size_t n) {
    buffer_.insert(buffer_.end(), buf, buf + n);
  }
};

class FileManager;

class MemoryTableView : public ReadableFile, nonmovable {
  mutable ReadableFileGuard file_;
  mutable MemoryTable *view_{};
  FileManager *parent_{};
  file_t id_;

public:
  MemoryTableView(MemoryTable *view, FileManager *parent)
      : view_(view), parent_(parent), id_(view->GetID()) {}

  ~MemoryTableView() override = default;

  void UpdateFile() const noexcept;

  ReadableFile *GetFile() const noexcept {
    return file_ != nullptr ? (ReadableFile *)file_.get()
                            : (ReadableFile *)view_;
  }

  uint64_t Size() const noexcept override {
    UpdateFile();
    ReadableFile *file = GetFile();
    return file ? file->Size() : -1;
  }

  Error GetError() const noexcept override {
    ReadableFile *file = GetFile();
    return file ? file->GetError() : Error{EBADFD};
  }

  ssize_t Read(uint64_t offset, char *buf, size_t n) override {
    UpdateFile();
    ReadableFile *file = GetFile();
    if (file == nullptr) {
      return -1;
    }
    return GetFile()->Read(offset, buf, n);
  }
};

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
  MemoryTable memtable_;
  record::Location latest_;

  Status OpenActiveFile(WritableFileGuard *file, file_t id);

  Status CreateActiveFile();

  Status Recovery(file_t active);

public:
  FileManager(MetadataManager *metadata, uint8_t max_open_files)
      : max_open_files_(max_open_files), memtable_(kMaxFileBytes),
        metadata_manager_(metadata) {}

  Status Init();

  auto AcquireLock() const noexcept { return std::unique_lock(mu_); }

  Status Sync() {
    auto status = Flush(true);
    if (status != Status::kOk) {
      return status;
    }

    auto err = active_->Sync();
    if (!err.Empty()) {
      return Status::kIOError;
    }
    return Status::kOk;
  }

  Status Flush(bool force) {
    if (memtable_.Size() == active_->GetOffset()) {
      return Status::kOk;
    }

    if (memtable_.Size() - active_->GetOffset() < kBlockSize) {
      if (!force) {
        return Status::kOk;
      }
    }

    uint64_t w = memtable_.Size() - active_->GetOffset();
    auto err = active_->Write(memtable_.Data() + active_->GetOffset(), w);
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to write active file: {}", err);
      return Status::kIOError;
    }
    return Status::kOk;
  }

  Status WriteActiveFile(Buffer *buffer, record::Location *loc) {
    if (latest_.offset + buffer->ReadableBytes() > kMaxFileBytes) {
      auto status = Flush(true);
      if (status != Status::kOk) {
        return status;
      }

      status = CreateActiveFile();
      if (status != Status::kOk) {
        return status;
      }
    }
    
    loc->offset = latest_.offset;
    loc->id = latest_.id;
    
    size_t w = buffer->ReadableBytes();
    memtable_.Append(buffer->ReadIndex(), w);
    buffer->Retrieve(w);
    latest_.offset += w;

    PEDRODB_IGNORE_ERROR(Flush(false));
    return Status::kOk;
  }

  Status ReleaseDataFile(file_t id);

  Status AcquireDataFile(file_t id, ReadableFileGuard *file);

  Error RemoveDataFile(file_t id) {
    if (id == latest_.id) {
      return Error::Success();
    }

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
