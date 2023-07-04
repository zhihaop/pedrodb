#include "pedrodb/db_impl.h"

namespace pedrodb {

Status DBImpl::Get(const ReadOptions &options, const std::string &key,
                   std::string *value) {
  auto rlock = AcquireReadLock();

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

  ArrayBuffer buffer;
  buffer.EnsureWriteable(metadata.length);
  file->Read(metadata.location.offset, buffer.Data() + buffer.WriteIndex(),
             metadata.length);
  buffer.Append(metadata.length);

  RecordHeader header{};
  if (!RecordHeader::Unpack(&header, &buffer)) {
    PEDRODB_ERROR("failed to unpack header");
    return Status::kIOError;
  }

  std::string key0(header.key_size, 0);
  buffer.Retrieve(key0.data(), key0.size());
  if (key0 != key) {
    PEDRODB_ERROR("key is not match: expected[{}], actual[{}]", key, key0);
    return Status::kCorruption;
  }

  if (header.timestamp != metadata.timestamp) {
    PEDRODB_ERROR("timestamp is not consistent: expect[{}], actual[{}]",
                  metadata.timestamp, header.timestamp);
    return Status::kCorruption;
  }

  if (header.type == RecordHeader::kSet) {
    value->resize(header.value_size, 0);
    buffer.Retrieve(value->data(), header.value_size);
    read_cache_->UpdateCache(metadata.location, *value);
    return Status::kOk;
  }

  return Status::kNotFound;
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
      options_.sync_interval, options_.sync_interval, [this] {
        auto lock = AcquireLock();
        auto fm_lock = file_manager_->AcquireLock();

        WritableFile *file;
        uint32_t id;
        file_manager_->GetActiveFile(&file, &id);

        PEDRODB_INFO("active file {} sync", id);
        file->Flush();
      });

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

  auto iter = RecordIterator(file.get());
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
  size_t expected = buf->ReadableBytes();
  ssize_t w = file->Write(buf);
  if (w != expected) {
    if (w < 0) {
      PEDRODB_ERROR("failed to write record: {}", Error{errno});
    } else {
      PEDRODB_ERROR("failed to write record: write[{}], expected[{}]", w,
                    expected);
    }
    return Status::kIOError;
  }

  if (options.sync) {
    auto err = file->Flush();
    if (!err.Empty()) {
      PEDRODB_ERROR("failed to flush data");
      return Status::kIOError;
    }
  }

  auto err = metadata_->Flush();
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

  std::unordered_map<std::string, std::string> data;
  std::string key;
  size_t records{};
  {
    auto rlock = AcquireReadLock();
    for (auto iter = RecordIterator(file.get()); iter.Valid(); records++) {
      auto next = iter.Next();

      key.assign(next.key);

      auto it = indices_.find(key);
      if (it == indices_.end()) {
        continue;
      }

      auto metadata = it->second;
      if (metadata.location.id != id) {
        continue;
      }

      if (metadata.location.offset != next.offset) {
        continue;
      }

      data[key].assign(next.value);
    }
  }

  if (records == data.size()) {
    return;
  }

  // put data into active file.
  {
    auto lock = AcquireLock();
    for (auto &[k, v] : data) {
      HandlePut({}, k, v);
    }
    compaction_state_.erase(id);
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
} // namespace pedrodb