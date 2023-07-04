#include "pedrodb/file_manager.h"

namespace pedrodb {

Status FileManager::OpenActiveFile(WritableFile **file, uint32_t id) {
  PEDRODB_INFO("create active file {}", id);
  auto status =
      WritableFile::OpenOrCreate(metadata_->GetDataFilePath(id), file);
  if (status != Status::kOk) {
    PEDRODB_ERROR("failed to load active file: {}",
                  metadata_->GetDataFilePath(id));
    return status;
  }
  PEDRODB_INFO("create active file: {}", metadata_->GetDataFilePath(id));
  return status;
}

Status FileManager::Init() {
  WritableFile *file;
  uint32_t id;
  auto file_count = metadata_->GetFileCount();
  if (file_count == 0) {
    return CreateActiveFile(&file, &id);
  }

  auto files = metadata_->GetFiles();
  id = *std::max_element(files.begin(), files.end());

  auto status = OpenActiveFile(&file, id);
  if (status != Status::kOk) {
    return status;
  }

  active_id_ = id;
  active_.reset(file);
  return Status::kOk;
}

Status FileManager::CreateActiveFile(WritableFile **file, uint32_t *id) {
  WritableFile *next_active = nullptr;
  uint32_t next_id = active_id_ + 1;

  auto stat = OpenActiveFile(&next_active, next_id);
  if (stat != Status::kOk) {
    return stat;
  }

  metadata_->CreateFile(next_id);
  metadata_->Flush();

  active_.reset(next_active);
  active_id_ = next_id;
  *file = next_active;
  *id = next_id;

  return stat;
}

Status FileManager::ReleaseDataFile(uint32_t id) {
  auto it = in_use_.find(id);
  if (it == in_use_.end()) {
    return Status::kInvalidArgument;
  }

  auto file = std::move(it->second);
  in_use_.erase(it);

  open_files_.update(id, std::move(file));
  return Status::kOk;
}

Status FileManager::AcquireDataFile(uint32_t id, ReadableFileGuard *file) {
  if (!metadata_->ExistFile(id)) {
    return Status::kInvalidArgument;
  }

  if (open_files_.contains(id)) {
    auto ptr = &(in_use_[id] = std::move(open_files_[id]));
    open_files_.erase(id);
    *file = ReadableFileGuard(ptr, [this, id](ReadableFile *ptr) {
      auto _ = AcquireLock();
      ReleaseDataFile(id);
    });
    return Status::kOk;
  }

  if (in_use_.count(id)) {
    return Status::kInvalidArgument;
  }

  std::string filename = metadata_->GetDataFilePath(id);
  ReadableFile f;
  auto stat = ReadableFile::Open(filename, &f);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }

  auto ptr = &(in_use_[id] = std::move(f));
  *file = ReadableFileGuard(ptr, [this, id](ReadableFile *ptr) {
    auto _ = AcquireLock();
    ReleaseDataFile(id);
  });
  return stat;
}
}