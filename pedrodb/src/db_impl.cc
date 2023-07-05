#include "pedrodb/db_impl.h"

namespace pedrodb {

using Executor = pedrolib::ThreadPoolExecutor;

Status DBImpl::Get(const ReadOptions &options, std::string_view key,
                   std::string *value) {
  auto lock = AcquireLock();
  return HandleGet(options, GetHash(key), key, value);
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

Status DBImpl::Put(const WriteOptions &options, std::string_view key,
                   std::string_view value) {
  auto lock = AcquireLock();
  return HandlePut(options, GetHash(key), key, value);
}

Status DBImpl::Delete(const WriteOptions &options, std::string_view key) {
  auto lock = AcquireLock();
  return HandlePut(options, GetHash(key), key, {});
}

Status DBImpl::Init() {
  Status status = metadata_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  status = file_manager_->Init();
  if (status != Status::kOk) {
    return status;
  }

  status = version_set_->Init();
  if (status != Status::kOk) {
    return status;
  }

  read_cache_ = std::make_unique<ReadCache>(options_.read_cache_bytes);
  read_cache_->UpdateActiveID(file_manager_->GetActiveFileID());
  executor_ = std::make_unique<Executor>(options_.executor_threads);

  status = Recovery();
  if (status != Status::kOk) {
    return status;
  }

  sync_worker_ = executor_->ScheduleEvery(
      options_.sync_interval, options_.sync_interval, [this] { Flush(); });

  return Status::kOk;
}

Status DBImpl::Compact() {
  std::vector<uint32_t> files;
  {
    auto _ = metadata_manager_->AcquireLock();
    auto all_files = metadata_manager_->GetFiles();
    files.assign(all_files.begin(), all_files.end());
  }
  for (auto f : files) {
    Compact(f);
  }
  return Status::kOk;
}

DBImpl::DBImpl(const Options &options, const std::string &name)
    : options_(options) {
  metadata_manager_ = std::make_unique<MetadataManager>(name);
  file_manager_ = std::make_unique<FileManager>(metadata_manager_.get(),
                                                options.max_open_files);
  version_set_ = std::make_unique<VersionSet>(metadata_manager_.get());
}

DBImpl::~DBImpl() {
  if (executor_) {
    executor_->ScheduleCancel(sync_worker_);
    executor_->Close();
  }
}

Status DBImpl::Recovery(uint32_t id) {
  ReadableFileGuard file;
  auto stat = file_manager_->AcquireDataFile(id, &file);
  if (stat != Status::kOk) {
    return stat;
  }

  size_t records = 0;
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

    KeyValueMetadata kv_metadata{.key = std::string{record.key},
                                 .metadata = metadata};

    size_t h = GetHash(record.key);
    if (record.type == RecordHeader::kSet) {
      indices_.emplace(h, kv_metadata);
      read_cache_->UpdateCache(location, record.value);
      records++;
    } else if (record.type == RecordHeader::kDelete) {
      auto it = GetMetadataIterator(h, record.key);
      if (it != indices_.end()) {
        compaction_state_[id].unused += it->second.metadata.length;
        indices_.erase(it);
      }
      read_cache_->Remove(location);
      records--;
    }
  }
  PEDRODB_TRACE("crash recover success: file[{}], record[{}]", id, records);
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

  // check compaction state.
  {
    auto lock = AcquireLock();
    auto it = compaction_state_.find(id);
    if (it == compaction_state_.end()) {
      return;
    }

    it->second.compacting = true;
  }

  PEDRODB_INFO("start compacting {}", id);

  struct CompactingRecord {
    std::string key;
    std::string value;
    uint32_t offset{};
  };

  std::unordered_multimap<size_t, CompactingRecord> records;
  ArrayBuffer buffer;
  for (auto iter = RecordIterator(file.get(), &buffer); iter.Valid();) {
    auto next = iter.Next();
    if (next.type != RecordHeader::kSet) {
      continue;
    }

    CompactingRecord record{
        .key = std::string{next.key},
        .value = std::string{next.value},
        .offset = next.offset,
    };

    size_t h = GetHash(next.key);
    records.emplace(h, std::move(record));
  }

  const size_t kBatchSize = 4096;

  // batch remove steal records.
  {
    auto last = records.begin();
    while (last != records.end()) {
      auto next = last;
      for (int i = 0; i < kBatchSize && next != records.end(); ++i) {
        ++next;
      }

      auto lock = AcquireLock();
      for (auto it = last; it != next;) {
        auto &[h, record] = *it;
        ValueLocation location{};
        location.id = id;
        location.offset = record.offset;
        if (CheckStealRecord(h, record.key, location)) {
          it = records.erase(it);
        } else {
          ++it;
        }
      }
      last = next;
    }
  }

  // batch append valid record.
  {
    auto last = records.begin();
    while (last != records.end()) {
      auto next = last;
      for (int i = 0; i < kBatchSize && next != records.end(); ++i) {
        ++next;
      }

      auto lock = AcquireLock();
      for (auto it = last; it != next; ++it) {
        auto &[h, record] = *it;
        WriteOptions options{.sync = it == records.end()};
        HandlePut(options, h, record.key, record.value);
        // update compaction state.
        if (it == records.end()) {
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

Status DBImpl::HandlePut(const WriteOptions &options, size_t h,
                         std::string_view key, std::string_view value) {

  size_t record_length = RecordHeader::SizeOf() + key.size() + value.size();
  if (record_length > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  // check valid deletion.
  if (value.empty()) {
    auto it = GetMetadataIterator(h, key);
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

  uint64_t timestamp = version_set_->IncreaseVersion();

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

  auto it = GetMetadataIterator(h, key);
  // replace or delete.
  if (it != indices_.end()) {
    auto &info = it->second;
    UpdateCompactionHint(info.metadata);
    read_cache_->Remove(info.metadata.location);

    if (value.empty()) {
      indices_.erase(it);
    } else {
      info.metadata = metadata;
      read_cache_->UpdateCache(metadata.location, value);
    }
    return Status::kOk;
  }

  // insert.
  indices_.emplace(h, KeyValueMetadata{
                          .key = std::string{key},
                          .metadata = metadata,
                      });
  read_cache_->UpdateCache(metadata.location, value);
  return Status::kOk;
}

bool DBImpl::CheckStealRecord(size_t h, std::string_view key,
                              ValueLocation location) {
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end()) {
    return true;
  }

  auto metadata = it->second.metadata;
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

    ValueLocation loc{};
    loc.id = l.id;
    loc.offset = next.offset;
    if (CheckStealRecord(GetHash(next.key), next.key, loc)) {
      continue;
    }

    read_cache_->UpdateCache(loc, next.value);
  }

  return found ? Status::kOk : Status::kCorruption;
}

Status DBImpl::Recovery() {
  auto all_files = metadata_manager_->GetFiles();
  std::vector<uint32_t> files(all_files.begin(), all_files.end());
  std::sort(files.begin(), files.end());

  for (auto file : files) {
    PEDRODB_TRACE("crash recover: file {}", file);
    auto status = Recovery(file);
    if (status != Status::kOk) {
      return status;
    }
  }
  file_manager_->ReleaseDataFile(files.back());
  return Status::kOk;
}

auto DBImpl::GetMetadataIterator(size_t h, std::string_view key)
    -> decltype(indices_.begin()) {
  auto [s, t] = indices_.equal_range(h);
  for (auto it = s; it != t; ++it) {
    if (it->second.key == key) {
      return it;
    }
  }
  return indices_.end();
}
Status DBImpl::HandleGet(const ReadOptions &options, size_t h,
                         std::string_view key, std::string *value) {
  auto it = GetMetadataIterator(h, key);
  if (it == indices_.end()) {
    return Status::kNotFound;
  }

  auto metadata = it->second.metadata;
  if (read_cache_->Read(metadata.location, value)) {
    return Status::kOk;
  }

  ReadableFileGuard file;
  {
    auto lock = file_manager_->AcquireLock();
    auto stat = file_manager_->AcquireDataFile(metadata.location.id, &file);
    if (stat != Status::kOk) {
      PEDRODB_ERROR("cannot get file {}", metadata.location.id);
      return stat;
    }
  }

  return FetchRecord(file.get(), metadata, value);
}
} // namespace pedrodb