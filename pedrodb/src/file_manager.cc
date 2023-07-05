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

  auto &files = metadata_->GetFiles();
  if (files.empty()) {
    auto status = CreateActiveFile(&file, &id);
    if (status != Status::kOk) {
      return status;
    }
  } else {
    id = *std::max_element(files.begin(), files.end());
    auto status = OpenActiveFile(&file, id);
    if (status != Status::kOk) {
      return status;
    }
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

  auto _ = metadata_->AcquireLock();
  metadata_->CreateFile(next_id);

  active_.reset(next_active);
  active_id_ = next_id;
  *file = next_active;
  *id = next_id;

  return stat;
}

Status FileManager::ReleaseDataFile(uint32_t id) {
  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::remove_if(open_files_.begin(), open_files_.end(), pred);
  open_files_.erase(it, open_files_.end());
  return Status::kOk;
}

Status FileManager::AcquireDataFile(uint32_t id, ReadableFileGuard *file) {
  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::find_if(open_files_.begin(), open_files_.end(), pred);
  if (it != open_files_.end()) {
    auto open_file = std::move(*it);
    open_files_.erase(it);
    *file = open_files_.emplace_back(std::move(open_file)).file;
    return Status::kOk;
  }
  
  *file = std::make_shared<ReadableFile>();
  std::string filename = metadata_->GetDataFilePath(id);
  auto stat = ReadableFile::Open(filename, file->get());
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }

  OpenFile open_file{id, *file};
  if (open_files_.size() == max_open_files_) {
    open_files_.erase(open_files_.begin());
    open_files_.emplace_back(std::move(open_file));
  }
  
  return stat;
}
} // namespace pedrodb