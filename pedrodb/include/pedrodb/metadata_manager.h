#ifndef PEDRODB_METADATA_MANAGER_H
#define PEDRODB_METADATA_MANAGER_H

#include "pedrodb/metadata_format.h"
#include "pedrodb/status.h"

#include <mutex>
#include <pedrolib/buffer/array_buffer.h>
#include <unordered_set>

namespace pedrodb {

class MetadataManager {
  mutable std::mutex mu_;

  uint32_t min_timestamp_{};
  uint32_t max_timestamp_{};
  std::string name_;
  std::unordered_set<uint32_t> files_;

  ArrayBuffer buffer_;
  File file_;

  const std::string path_;

  Status Recovery() {
    buffer_.EnsureWriteable(File::Size(file_));
    buffer_.Append(&file_);

    MetadataHeader header;
    header.UnPack(&buffer_);
    min_timestamp_ = header.timestamp;
    max_timestamp_ = min_timestamp_;
    name_ = header.name;

    PEDRODB_INFO("read database {}", name_);

    while (buffer_.ReadableBytes()) {
      MetadataChangeLogEntry logEntry;
      logEntry.UnPack(&buffer_);

      if (logEntry.type == kCreateFile) {
        files_.emplace(logEntry.id);
      } else {
        files_.erase(logEntry.id);
      }
    }

    return Status::kOk;
  }

  Status CreateDatabase() {
    MetadataHeader header;
    header.name = name_;
    header.timestamp = 0;

    min_timestamp_ = 0;
    max_timestamp_ = 0;

    header.Pack(&buffer_);
    buffer_.Retrieve(&file_);
    file_.Sync();
    return Status::kOk;
  }

public:
  explicit MetadataManager(std::string path) : path_(std::move(path)) {}
  ~MetadataManager() = default;

  Status Init() {
    size_t index = path_.find_last_of(".db");
    if (index != path_.size() - 1) {
      PEDRODB_ERROR("db filename[{}] error", path_);
      return Status::kInvalidArgument;
    }

    name_ = path_.substr(0, path_.size() - 3);

    File::OpenOption option{.mode = File::OpenMode::kReadWrite, .create = 0777};
    file_ = File::Open(path_.c_str(), option);
    if (!file_.Valid()) {
      PEDRODB_ERROR("cannot open filename[{}]: {}", path_, file_.GetError());
      return Status::kIOError;
    }

    int64_t filesize = File::Size(file_);
    if (filesize < 0) {
      PEDRODB_ERROR("cannot get filesize[{}]: {}", path_, file_.GetError());
      return Status::kIOError;
    }

    if (filesize != 0) {
      return Recovery();
    }

    PEDRODB_INFO("db[{}] not exist, create one", name_);
    return CreateDatabase();
  }

  void AcquireVersion() {
    if (min_timestamp_ == max_timestamp_) {
      if (buffer_.WritableBytes()) {
        buffer_.Retrieve(&file_);
      }
      PEDRODB_INFO("acquire timestamp");
      uint32_t timestamp = htobe(max_timestamp_ + kBatchVersions);
      file_.Pwrite(0, &timestamp, sizeof(timestamp));
      file_.Sync();
    }
  }

  uint64_t AddVersion() {
    std::unique_lock<std::mutex> lock(mu_);
    AcquireVersion();
    return ++min_timestamp_;
  }

  bool ExistFile(uint32_t id) {
    std::unique_lock<std::mutex> lock(mu_);
    return files_.count(id);
  }

  std::vector<uint32_t> GetFiles() {
    std::unique_lock<std::mutex> lock(mu_);
    return {files_.begin(), files_.end()};
  }

  Status CreateFile(uint32_t id) {
    std::unique_lock<std::mutex> lock(mu_);
    if (files_.count(id)) {
      return Status::kOk;
    }
    files_.emplace(id);

    MetadataChangeLogEntry entry;
    entry.type = kCreateFile;
    entry.id = id;

    entry.Pack(&buffer_);
    return Status::kOk;
  }

  Status DeleteFile(uint32_t id) {
    std::unique_lock<std::mutex> lock(mu_);
    if (!files_.count(id)) {
      return Status::kOk;
    }
    files_.erase(id);

    MetadataChangeLogEntry entry;
    entry.type = kDeleteFile;
    entry.id = id;

    entry.Pack(&buffer_);
    return Status::kOk;
  }

  size_t GetFileCount() const noexcept {
    std::unique_lock<std::mutex> lock(mu_);
    return files_.size();
  }

  Error handleFlush() {
    if (buffer_.WritableBytes()) {
      buffer_.Retrieve(&file_);
    }
    return Error::Success();
  }

  Error Flush() {
    std::unique_lock<std::mutex> lock(mu_);
    return handleFlush();
  }

  std::string GetDataFilePath(uint32_t id) {
    return fmt::format("{}_{}.data", name_, id);
  }
};

} // namespace pedrodb

#endif // PEDRODB_METADATA_MANAGER_H
