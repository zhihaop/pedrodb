#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions &options, const std::string &key,
                   std::string *value) {
  auto lock = AcquireLock();

  auto it = indices_.find(key);
  if (it == indices_.end()) {
    return Status::kNotFound;
  }

  auto metadata = it->second;
  if (read_cache_->Read(metadata.location, value)) {
    return Status::kOk;
  }

  ReadableFileGuard file;
  {
    auto _ = file_manager_->AcquireLock();
    auto stat = file_manager_->AcquireDataFile(metadata.location.id, &file);
    if (stat != Status::kOk) {
      PEDRODB_ERROR("cannot get file {}", metadata.location.id);
      return stat;
    }
  }

  return FetchRecord(file.get(), metadata, value);
}

Status DBImpl::GetActiveFile(WritableFile **file, uint32_t *id,
                             size_t record_length) {
  auto lock = file_manager_->AcquireLock();
  file_manager_->GetActiveFile(file, id);

  if ((*file)->GetOffSet() + record_length <= kMaxFileBytes) {
    return Status::kOk;
  }

  auto status = file_manager_->CreateActiveFile(file, id);
  if (status != Status::kOk) {
    return status;
  }
  read_cache_->UpdateActiveID(*id);
  return Status::kOk;
}

void DBImpl::UpdateCompactionHint(ValueMetadata metadata) {
  auto &state = compaction_state_[metadata.location.id];
  if (state.compacting) {
    return;
  }

  state.unused += metadata.length;
  if (state.unused > options_.compaction_threshold_bytes) {
    state.compacting = true;
    executor_->Schedule([=] { Compact(metadata.location.id); });
    PEDRODB_INFO("schedule compact {} {}", metadata.location.id, state.unused);
  }
}

Status DBImpl::Put(const WriteOptions &options, const std::string &key,
                   std::string_view value) {
  auto lock = AcquireLock();
  return HandlePut(options, key, value);
}

Status DBImpl::Delete(const WriteOptions &options, const std::string &key) {
  auto lock = AcquireLock();
  return HandlePut(options, key, {});
}

Status DBImpl::Init() {
  Status status = metadata_->Init();
  if (status != Status::kOk) {
    return status;
  }

  status = file_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  read_cache_ = std::make_unique<ReadCache>(options_.read_cache_bytes);
  read_cache_->UpdateActiveID(file_manager_->GetActiveFileID());
  executor_ =
      std::make_unique<pedrolib::ThreadPoolExecutor>(options_.executor_threads);

  PEDRODB_INFO("crash recover start");
  auto files = metadata_->GetFiles();
  std::sort(files.begin(), files.end());

  for (auto file : files) {
    RebuildIndices(file);
  }
  file_manager_->Close(files.back());
  PEDRODB_INFO("crash recover finished");

  sync_worker_ = executor_->ScheduleEvery(
      options_.sync_interval, options_.sync_interval, [this] { Flush(); });

  return Status::kOk;
}

Status DBImpl::Compact() {
  auto files = metadata_->GetFiles();
  std::sort(files.begin(), files.end());
  for (int i = 0; i < files.size(); ++i) {
    Compact(i);
  }
  metadata_->Flush();
  return Status::kOk;
}

DBImpl::DBImpl(const Options &options, const std::string &name)
    : options_(options) {
  metadata_ = std::make_unique<MetadataManager>(name);
  file_manager_ =
      std::make_unique<FileManager>(metadata_.get(), options.max_open_files);
}

DBImpl::~DBImpl() {
  if (executor_) {
    executor_->ScheduleCancel(sync_worker_);
    executor_->Close();
  }
}

Status DBImpl::RebuildIndices(uint32_t id) {
  ReadableFileGuard file;
  auto stat = file_manager_->AcquireDataFile(id, &file);
  if (stat != Status::kOk) {
    return stat;
  }

  buffer_.Reset();
  auto iter = RecordIterator(file.get(), &buffer_);
  while (iter.Valid()) {
    auto record = iter.Next();
    ValueLocation location{
        .id = id,
        .offset = record.offset,
    };

    ValueMetadata metadata{
        .location = location,
        .length = record.length,
        .timestamp = record.timestamp,
    };

    if (record.type == RecordHeader::kSet) {
      indices_[std::string{record.key}] = metadata;
      read_cache_->UpdateCache(location, record.value);
    } else if (record.type == RecordHeader::kDelete) {
      indices_.erase(std::string{record.key});
      read_cache_->Remove(location);
    }
  }
  PEDRODB_IGNORE_ERROR(file_manager_->ReleaseDataFile(id));
  return Status::kOk;
}

Status DBImpl::WriteDisk(Buffer *buf, WritableFile *file,
                         WriteOptions options) {
  auto err = file->Write(buf);
  if (!err.Empty()) {
    PEDRODB_ERROR("failed to write record: {}", err);
    return Status::kIOError;
  }

  if (options.sync) {
    err = file->Flush();
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to flush data: {}", err);
    }

    err = file->Sync();
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to sync data: {}", err);
      return Status::kIOError;
    }
  }

  err = metadata_->Flush();
  if (!err.Empty()) {
    PEDRODB_ERROR("failed to flush metadata");
    return Status::kIOError;
  }

  return Status::kOk;
}

void DBImpl::Compact(uint32_t id) {
  ReadableFileGuard file;
  {
    auto _ = file_manager_->AcquireLock();
    if (file_manager_->GetActiveFileID() == id) {
      return;
    }

    auto stat = file_manager_->AcquireDataFile(id, &file);
    if (stat != Status::kOk) {
      return;
    }
  }

  PEDRODB_INFO("start compacting {}", id);

  struct CompactingValue {
    std::string value;
    uint32_t offset{};
  };

  std::unordered_map<std::string, CompactingValue> values;
  std::string key;
  size_t records{};
  ArrayBuffer buffer;
  for (auto iter = RecordIterator(file.get(), &buffer); iter.Valid();
       records++) {
    auto next = iter.Next();
    key.assign(next.key);
    auto &value = values[key];
    value.value.assign(next.value);
    value.offset = next.offset;
  }

  const size_t kBatchSize = 4096;

  // batch remove steal records.
  {
    auto last = values.begin();
    while (last != values.end()) {
      auto next = last;
      for (int i = 0; i < kBatchSize && next != values.end(); ++i) {
        ++next;
      }

      auto lock = AcquireLock();
      for (auto it = last; it != next;) {
        ValueLocation location{};
        location.id = id;
        location.offset = it->second.offset;
        if (CheckStealRecord(it->first, location)) {
          it = values.erase(it);
        } else {
          ++it;
        }
      }
      last = next;
    }
  }

  // batch append valid record.
  {
    auto last = values.begin();
    while (last != values.end()) {
      auto next = last;
      for (int i = 0; i < kBatchSize && next != values.end(); ++i) {
        ++next;
      }

      auto lock = AcquireLock();
      for (auto it = last; it != next; ++it) {
        WriteOptions options{.sync = it == values.end()};
        HandlePut(options, it->first, it->second.value);
        // update compaction state.
        if (it == values.end()) {
          compaction_state_.erase(id);
        }
      }
      last = next;
    }
  }

  // remove file.
  {
    auto _ = file_manager_->AcquireLock();
    PEDRODB_IGNORE_ERROR(file_manager_->ReleaseDataFile(id));
    PEDRODB_IGNORE_ERROR(file_manager_->RemoveDataFile(id));
  }
}

Status DBImpl::HandlePut(const WriteOptions &options, const std::string &key,
                         std::string_view value) {

  size_t record_length = RecordHeader::SizeOf() + key.size() + value.size();
  if (record_length > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  // check valid deletion.
  if (value.empty()) {
    auto it = indices_.find(key);
    if (it == indices_.end()) {
      return Status::kNotFound;
    }
  }

  WritableFile *active;
  uint32_t id;
  auto status = GetActiveFile(&active, &id, record_length);
  if (status != Status::kOk) {
    return status;
  }

  uint64_t timestamp = metadata_->AddVersion();

  ValueMetadata metadata;
  metadata.timestamp = timestamp;
  metadata.length = record_length;
  metadata.location.id = id;
  metadata.location.offset = active->GetOffSet();

  RecordHeader header{};
  header.key_size = key.size();
  header.type = value.empty() ? RecordHeader::kDelete : RecordHeader::kSet;
  header.value_size = value.size();
  header.timestamp = timestamp;
  header.crc32 = 0;

  buffer_.Reset();
  header.Pack(&buffer_);
  buffer_.Append(key.data(), key.size());
  buffer_.Append(value.data(), value.size());
  status = WriteDisk(&buffer_, active, options);

  if (status != Status::kOk) {
    return status;
  }

  auto it = indices_.find(key);
  // replace or delete.
  if (it != indices_.end()) {
    UpdateCompactionHint(it->second);
    read_cache_->Remove(it->second.location);

    if (value.empty()) {
      indices_.erase(it);
    } else {
      it->second = metadata;
      read_cache_->UpdateCache(metadata.location, value);
    }
    return Status::kOk;
  }

  // insert.
  indices_.emplace_hint(it, key, metadata);
  read_cache_->UpdateCache(metadata.location, value);
  return Status::kOk;
}

bool DBImpl::CheckStealRecord(const std::string &key, ValueLocation location) {
  auto it = indices_.find(key);
  if (it == indices_.end()) {
    return true;
  }

  auto metadata = it->second;
  if (metadata.location.id != location.id) {
    return true;
  }

  if (metadata.location.offset != location.offset) {
    return true;
  }

  return false;
}

Status DBImpl::Flush() {
  auto lock = AcquireLock();
  auto fm_lock = file_manager_->AcquireLock();

  WritableFile *file;
  uint32_t id;
  file_manager_->GetActiveFile(&file, &id);

  // TODO impl write guard.
  PEDRODB_INFO("active file {} sync", id);
  auto err = file->Flush();
  if (!err.Empty()) {
    PEDRODB_ERROR("failed to flush file: {}", err);
    return Status::kIOError;
  }

  err = file->Sync();
  if (!err.Empty()) {
    PEDRODB_ERROR("failed to sync file: {}", err);
    return Status::kIOError;
  }
  return Status::kOk;
}

Status DBImpl::FetchRecord(ReadableFile *file, ValueMetadata metadata,
                           std::string *value) {
  ValueLocation l = metadata.location;
  uint32_t filesize = file->Size();

  uint32_t sentry = std::min(l.offset + metadata.length, filesize);

  if (options_.prefetch) {
    sentry = std::min(sentry - (sentry % kPageSize) + kPageSize, filesize);
  }

  buffer_.Reset();
  std::string key;
  auto it = RecordIterator(file, sentry, &buffer_);
  it.SetOffset(l.offset);

  // read the whole page.
  bool first = true;
  bool found = false;
  while (it.Valid()) {
    auto next = it.Next();
    if (next.type != RecordHeader::kSet) {
      continue;
    }

    if (metadata.timestamp != next.timestamp) {
      continue;
    }

    if (first) {
      read_cache_->UpdateCache(l, next.value);
      value->assign(next.value);
      first = false;
      found = true;
      continue;
    }

    key.assign(next.key);
    ValueLocation loc{};
    loc.id = l.id;
    loc.offset = next.offset;
    if (CheckStealRecord(key, loc)) {
      continue;
    }

    read_cache_->UpdateCache(loc, next.value);
  }

  return found ? Status::kOk : Status::kNotFound;
}
} // namespace pedrodb