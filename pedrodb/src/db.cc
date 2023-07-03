#include "pedrodb/db.h"
#include "pedrodb/db_impl.h"
#include "pedrodb/logger/logger.h"

namespace pedrodb {
Status DB::Open(const Options &options, const std::string &name,
                std::shared_ptr<DB> *db) {
  PEDRODB_INFO("Open database {}", name);

  auto impl = std::make_shared<DBImpl>();
  impl->metadata_ = std::make_unique<MetadataManager>();

  auto stat = impl->metadata_->Open(name);
  if (stat != Status::kOk) {
    return stat;
  }

  impl->file_manager_ = std::make_unique<FileManager>(impl->metadata_.get(),
                                                      options.max_open_files);

  stat = impl->Init();
  if (stat != Status::kOk) {
    return stat;
  }

  *db = impl;
  return Status::kOk;
}
} // namespace pedrodb