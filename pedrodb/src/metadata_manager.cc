#include "pedrodb/metadata_manager.h"
#include "pedrodb/defines.h"
#include "pedrodb/logger/logger.h"
namespace pedrodb {

Status MetadataManager::Recovery() {
  ArrayBuffer buffer(File::Size(file_));
  if (buffer.Append(&file_) != buffer.Capacity()) {
    PEDRODB_FATAL("failed to read file: {}", file_.GetError());
  }

  metadata::Header header;
  if (!header.UnPack(&buffer)) {
    PEDRODB_FATAL("failed to read header");
  }
  name_ = header.name;

  PEDRODB_INFO("read database {}", name_);

  while (buffer.ReadableBytes()) {
    metadata::LogEntry logEntry;
    if (!logEntry.UnPack(&buffer)) {
      PEDRODB_FATAL("failed to open db");
    }

    if (logEntry.type == metadata::LogType::kCreateFile) {
      files_.emplace(logEntry.id);
    } else {
      files_.erase(logEntry.id);
    }
  }

  return Status::kOk;
}
Status MetadataManager::CreateDatabase() {
  metadata::Header header;
  header.name = name_;

  ArrayBuffer buffer(metadata::Header::SizeOf(name_.size()));
  header.Pack(&buffer);
  buffer.Retrieve(&file_);
  file_.Sync();
  return Status::kOk;
}

Status MetadataManager::Init() {
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

Status MetadataManager::CreateFile(file_t id) {
  auto lock = AcquireLock();
  if (files_.count(id)) {
    return Status::kOk;
  }
  files_.emplace(id);

  metadata::LogEntry entry;
  entry.type = metadata::LogType::kCreateFile;
  entry.id = id;

  ArrayBuffer slice(metadata::LogEntry::SizeOf());
  entry.Pack(&slice);
  slice.Retrieve(&file_);

  PEDRODB_IGNORE_ERROR(file_.Sync());
  return Status::kOk;
}

Status MetadataManager::DeleteFile(file_t id) {
  auto lock = AcquireLock();
  auto it = files_.find(id);
  if (it == files_.end()) {
    return Status::kOk;
  }
  files_.erase(it);

  metadata::LogEntry entry;
  entry.type = metadata::LogType::kDeleteFile;
  entry.id = id;

  ArrayBuffer slice(metadata::LogEntry::SizeOf());
  entry.Pack(&slice);
  slice.Retrieve(&file_);

  PEDRODB_IGNORE_ERROR(file_.Sync());
  return Status::kOk;
}

std::string MetadataManager::GetDataFilePath(file_t id) const noexcept {
  return fmt::format("{}_{}.data", name_, id);
}
}  // namespace pedrodb