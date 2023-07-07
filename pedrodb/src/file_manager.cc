#include "pedrodb/file_manager.h"

namespace pedrodb {

Status FileManager::OpenActiveFile(WritableFileGuard *file, file_t id) {
  PEDRODB_INFO("create active file {}", id);
  auto path = metadata_manager_->GetDataFilePath(id);
  auto status = WritableFile::OpenOrCreate(path, file);
  if (status != Status::kOk) {
    PEDRODB_ERROR("failed to load active file: {}",
                  metadata_manager_->GetDataFilePath(id));
    return status;
  }
  PEDRODB_INFO("create active file: {}",
               metadata_manager_->GetDataFilePath(id));
  return status;
}

Status FileManager::Recovery(file_t active) {
  WritableFileGuard file;
  auto status = OpenActiveFile(&file, active);
  if (status != Status::kOk) {
    return status;
  }

  std::string value;
  auto err = file->ReadAll(&value);
  if (!err.Empty()) {
    return Status::kIOError;
  }

  active_ = std::move(file);
  memtable_ = std::move(value);
  
  latest_.id = active;
  latest_.offset = memtable_.size();

  return Status::kOk;
}

Status FileManager::Init() {
  auto &files = metadata_manager_->GetFiles();
  if (files.empty()) {
    auto status = CreateActiveFile();
    if (status != Status::kOk) {
      return status;
    }
    return Status::kOk;
  }

  return Recovery(*std::max_element(files.begin(), files.end()));
}

Status FileManager::CreateActiveFile() {
  WritableFileGuard active;
  file_t id = latest_.id + 1;

  auto stat = OpenActiveFile(&active, id);
  if (stat != Status::kOk) {
    return stat;
  }

  auto _ = metadata_manager_->AcquireLock();
  metadata_manager_->CreateFile(id);
  active_ = std::move(active);
  latest_.id = id;
  latest_.offset = 0;
  memtable_.clear();

  return Status::kOk;
}

void FileManager::ReleaseDataFile(file_t id) {
  if (id == latest_.id) {
    return;
  }

  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::remove_if(open_files_.begin(), open_files_.end(), pred);
  open_files_.erase(it, open_files_.end());
}

Status FileManager::AcquireDataFile(file_t id, ReadableFileGuard *file) {
  if (id == latest_.id) {
    *file = std::make_shared<MemoryTableView>(&memtable_, this);
    return Status::kOk;
  }

  // find the opened file with id
  auto pred = [id](const OpenFile &file) { return file.id == id; };
  auto it = std::find_if(open_files_.begin(), open_files_.end(), pred);
  if (it != open_files_.end()) {
    // lru strategy.
    auto open_file = std::move(*it);
    open_files_.erase(it);
    *file = open_files_.emplace_back(std::move(open_file)).file;
    return Status::kOk;
  }

  ReadableFileImpl f;
  std::string filename = metadata_manager_->GetDataFilePath(id);
  auto stat = ReadableFileImpl::Open(filename, &f);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot open file[name={}, id={}]", filename, id);
    return stat;
  }
  *file = std::make_shared<ReadableFileImpl>(std::move(f));

  OpenFile open_file{id, *file};
  if (open_files_.size() == max_open_files_) {
    open_files_.erase(open_files_.begin());
    open_files_.emplace_back(std::move(open_file));
  }

  return stat;
}

Error FileManager::RemoveDataFile(file_t id) {
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

Status FileManager::WriteActiveFile(Buffer *buffer, record::Location *loc) {
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
  memtable_.insert(memtable_.end(), buffer->ReadIndex(),
                   buffer->ReadIndex() + w);
  buffer->Retrieve(w);
  latest_.offset += w;

  PEDRODB_IGNORE_ERROR(Flush(false));
  return Status::kOk;
}

Status FileManager::Flush(bool force) {
  if (memtable_.size() == active_->GetOffset()) {
    return Status::kOk;
  }

  if (memtable_.size() - active_->GetOffset() < kBlockSize) {
    if (!force) {
      return Status::kOk;
    }
  }

  if (memtable_.size() < active_->GetOffset()) {
    PEDRODB_FATAL("should not happened: {} {}", memtable_.size(),
                  active_->GetOffset());
  }

  uint64_t w = memtable_.size() - active_->GetOffset();
  auto err = active_->Write(memtable_.data() + active_->GetOffset(), w);
  if (!err.Empty()) {
    PEDRODB_ERROR("failed to write active file: {}", err);
    return Status::kIOError;
  }
  return Status::kOk;
}

Status FileManager::Sync() {
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

void MemoryTableView::UpdateFile() const noexcept {
  if (parent_->GetActiveFile() == id_) {
    return;
  }
  memtable_ = nullptr;

  auto status = parent_->AcquireDataFile(id_, &file_);
  if (status != Status::kOk) {
    return;
  }
}

uint64_t MemoryTableView::Size() const noexcept {
  auto lock = parent_->AcquireLock();
  UpdateFile();

  if (memtable_) {
    return memtable_->size();
  }
  lock.unlock();

  if (file_) {
    return file_->Size();
  }
  return -1;
}

Error MemoryTableView::GetError() const noexcept {
  if (memtable_) {
    return Error::kOk;
  }
  if (file_) {
    return file_->GetError();
  }
  return Error{EBADFD};
}

ssize_t MemoryTableView::Read(uint64_t offset, char *buf, size_t n) {
  auto lock = parent_->AcquireLock();
  UpdateFile();

  if (memtable_ != nullptr) {
    uint64_t sentry = std::min(offset + n, (uint64_t)memtable_->size());
    std::copy(memtable_->data() + offset, memtable_->data() + sentry, buf);
    return static_cast<ssize_t>(sentry - offset);
  }
  lock.unlock();

  if (file_) {
    file_->Read(offset, buf, n);
  }
  return -1;
}

} // namespace pedrodb