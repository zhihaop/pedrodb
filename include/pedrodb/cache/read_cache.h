#ifndef PEDRODB_CACHE_READ_CACHE_H
#define PEDRODB_CACHE_READ_CACHE_H

#include <variant>
#include "pedrodb/cache/lru_cache.h"
#include "pedrodb/cache/segment_cache.h"
#include "pedrodb/file/readable_file.h"
#include "pedrodb/format/record_format.h"
#include "pedrodb/options.h"

namespace pedrodb {

class ReadableView final {
  const char* buf_;

  size_t read_index_;
  size_t write_index_;

 public:
  ReadableView(const char* buf, size_t length)
      : buf_(buf), read_index_(0), write_index_(length) {}

  [[nodiscard]] size_t ReadableBytes() const noexcept {
    return write_index_ - read_index_;
  }
  [[nodiscard]] const char* ReadIndex() const noexcept {
    return read_index_ + buf_;
  }
  void Retrieve(size_t n) noexcept { read_index_ += n; }
};

class ReadCache {

  struct Block {
    constexpr static size_t kBit = 12;
    using Ptr = std::shared_ptr<Block>;

    std::array<char, (1 << kBit)> data_;

    std::string_view substr(size_t left, size_t length) {
      return {data_.data() + left, length};
    }

    char* data() noexcept { return data_.data(); }
    [[nodiscard]] size_t size() const noexcept { return data_.size(); }
  };

  static uint32_t GetOffset(uint64_t block_idx) {
    return static_cast<uint32_t>(block_idx << Block::kBit);
  }

  static file_id_t GetFile(uint64_t block_idx) {
    return static_cast<uint32_t>((block_idx << Block::kBit) >> 32);
  }

  static uint64_t GetBlockIdx(file_id_t file_idx, uint32_t offset) {
    return ((static_cast<uint64_t>(file_idx) << 32) | offset) >> Block::kBit;
  }

 public:
  class Context {
    friend class ReadCache;

    ReadableFile::Ptr file_;

    file_id_t file_idx_;
    uint32_t begin_;
    uint32_t end_;

    record::EntryView entry_;

    std::string buf_;
    std::string_view block_ref_view_;
    Block::Ptr block_ref_;

   public:
    Context(const record::Location& loc, size_t length)
        : file_idx_(loc.id), begin_(loc.offset), end_(loc.offset + length) {}

    Status Build() {
      std::string_view buf;
      if (block_ref_ != nullptr) {
        buf = block_ref_view_;
      } else {
        buf = buf_;
      }

      ReadableView view(buf.data(), buf.size());
      return entry_.UnPack(&view) ? Status::kOk : Status::kCorruption;
    }

    [[nodiscard]] size_t Size() const noexcept { return end_ - begin_; }

    [[nodiscard]] auto GetEntry() const noexcept { return entry_; }
  };

  Status Get(uint64_t block_idx, Context& ctx) {
    Block::Ptr block = std::make_shared<Block>();
    Status stat =
        block_cache_.GetOrCompute(block_idx, block, [block_idx, &ctx, this] {
          auto block = std::make_shared<Block>();
          if (ctx.file_ == nullptr) {
            if (Status stat = file_opener_(GetFile(block_idx), &ctx.file_);
                stat != Status::kOk) {
              return std::pair{stat, block};
            }
          }

          ssize_t r = ctx.file_->Read(GetOffset(block_idx), block->data(),
                                      block->size());
          if (r != block->size()) {
            return std::pair{Status::kIOError, block};
          }

          return std::pair{Status::kOk, block};
        });

    if (stat != Status::kOk) {
      return stat;
    }

    uint32_t begin = std::max(GetOffset(block_idx), ctx.begin_);
    uint32_t end = std::min(GetOffset(block_idx + 1), ctx.end_);

    std::string_view slice =
        block->substr(begin - GetOffset(block_idx), end - begin);
    if (slice.size() == ctx.Size()) {
      ctx.block_ref_ = block;
      ctx.block_ref_view_ = slice;
    } else {
      if (ctx.buf_.capacity() < ctx.Size()) {
        ctx.buf_.reserve(ctx.Size());
      }
      ctx.buf_.append(slice);
    }
    return Status::kOk;
  }

 public:
  ReadCache(size_t segments, size_t capacity) : block_cache_(segments) {
    size_t segment_capacity = (capacity + segments - 1) / segments;
    for (size_t i = 0; i < segments; ++i) {
      block_cache_.SegmentAdd(segment_capacity >> Block::kBit);
    }
  }

  explicit ReadCache(const ReadCacheOptions& options)
      : ReadCache(options.segments, options.read_cache_bytes) {}

  Status Get(Context& ctx) {
    uint64_t blockIdx = GetBlockIdx(ctx.file_idx_, ctx.begin_);
    for (uint64_t i = blockIdx; GetOffset(i) < ctx.end_; ++i) {
      if (auto stat = Get(i, ctx); stat != Status::kOk) {
        return stat;
      }
    }

    return ctx.Build();
  }

  void SetFileOpener(
      std::function<Status(file_id_t, ReadableFile::Ptr*)> opener) {
    file_opener_ = std::move(opener);
  }

 private:
  SegmentCache<LRUCache<uint64_t, Block::Ptr>> block_cache_;
  std::function<Status(file_id_t, ReadableFile::Ptr*)> file_opener_;
};
}  // namespace pedrodb

#endif  //PEDRODB_CACHE_READ_CACHE_H
