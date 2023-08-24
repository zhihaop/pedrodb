#ifndef PEDRODB_CACHE_READ_CACHE_H
#define PEDRODB_CACHE_READ_CACHE_H

#include <shared_mutex>
#include "pedrodb/cache/lru_cache.h"
#include "pedrodb/cache/segment_cache.h"
#include "pedrodb/cache/simple_lru_cache.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/format/record_format.h"
#include "pedrodb/options.h"

namespace pedrodb {

class ReadContext {
  ArrayBuffer buf_;
  ReadableFile::Ptr file_;

  uint64_t begin_;
  uint64_t end_;

 public:
  ReadContext(uint64_t begin, uint64_t end)
      : buf_(end - begin), begin_(begin), end_(end) {}
};

class ReadCache {
  constexpr static size_t kBlockSize = 4 << 10;

  struct Block {
    using Ptr = std::shared_ptr<Block>;

    std::array<char, kBlockSize> data_;

    char* data() noexcept { return data_.data(); }
    [[nodiscard]] size_t size() const noexcept { return data_.size(); }
  };

 public:
  class Context {
    friend class ReadCache;

    ArrayBuffer buf_;
    ReadableFile::Ptr file_;

    uint64_t begin_;
    uint64_t end_;

    record::EntryView entry_;

    std::shared_mutex mu_;

   public:
    Context(const record::Location& loc, size_t length)
        : buf_(length), begin_(loc.encode()), end_(loc.encode() + length) {}

    Status Build() {
      return entry_.UnPack(&buf_) ? Status::kOk : Status::kCorruption;
    }

    auto wlock() { return std::unique_lock{mu_}; }
    auto rlock() { return std::shared_lock{mu_}; }

    [[nodiscard]] auto GetEntry() const noexcept { return entry_; }
  };

  Status Get(uint64_t loc, size_t /* offset */, Context& ctx) {
    Block::Ptr b;
    if (!block_cache_.Get(loc, b)) {
      if (ctx.file_ == nullptr) {
        auto status = file_opener_(GetFile(loc), &ctx.file_);
        if (status != Status::kOk) {
          return status;
        }
      }
      
      // TODO(zhihaop): using wlock to avoid read hotspot.
      b = std::make_shared<Block>();
      ssize_t r = ctx.file_->Read(GetOffset(loc), b->data(), b->size());
      if (r != b->size()) {
        return Status::kIOError;
      }

      block_cache_.Put(loc, b);
    }

    uint64_t begin = std::max(loc, ctx.begin_);
    uint64_t end = std::min(loc + kBlockSize, ctx.end_);

    if (end - begin > 0) {
      ctx.buf_.Append(b->data() + (begin - loc), end - begin);
    }
    return Status::kOk;
  }

  static file_id_t GetFile(uint64_t loc) noexcept { return loc >> 32; }

  static uint32_t GetOffset(uint64_t loc) noexcept { return loc; }

 public:
  ReadCache(size_t segments, size_t capacity) : block_cache_(segments) {
    size_t segment_capacity = (capacity + segments - 1) / segments;
    for (size_t i = 0; i < segments; ++i) {
      block_cache_.SegmentAdd(segment_capacity);
    }
  }

  explicit ReadCache(const ReadCacheOptions& options)
      : ReadCache(options.segments, options.read_cache_bytes) {}

  Status Get(Context& ctx) {
    uint64_t align_loc = ctx.begin_ / kBlockSize * kBlockSize;
    size_t length = ctx.buf_.Capacity() + ctx.begin_ - align_loc;
    size_t blocks = (length + kBlockSize - 1) / kBlockSize;
    for (size_t i = 0; i < blocks; ++i) {
      size_t offset = i * kBlockSize;
      auto status = Get(align_loc + offset, offset, ctx);
      if (status != Status::kOk) {
        return status;
      }
    }

    return ctx.Build();
  }

  void SetFileOpener(
      std::function<Status(file_id_t, ReadableFile::Ptr*)> opener) {
    file_opener_ = std::move(opener);
  }

 private:
  SegmentCache<SimpleLRUCache<uint64_t, Block::Ptr>> block_cache_;
  std::function<Status(file_id_t, ReadableFile::Ptr*)> file_opener_;
};
}  // namespace pedrodb

#endif  //PEDRODB_CACHE_READ_CACHE_H
