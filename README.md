# Pedrokv: A fast and reliable key-value storage service using Bitcask Model
## 简介
### BitCask 存储模型
BitCask 是一个面向磁盘的 KeyValue 存储模型，它的特性有：

- 单点 Get
- 单点 Set / 批量 Set
- 多版本 Value
- 不支持范围扫描 Scan

它设计的主要精髓有：

- Append-Only：所有写操作变成顺序操作
- Memory-Index：从 Key 到 Value 的索引存储在内存中
- Compact：通过压实清除无用数据，只有一个文件可写，其余只读
### PedroDB
PedroDB 在 BitCask 模型上实现了一套高性能的 KeyValue 存储系统，对比 LevelDB 和 RocksDB，我们在（单点/批量）写入和（单点/随机）读取上具有一定的优势，在 16B key 和 100B value 的测试下：

- 单点/随机 Get：均匀分布，120wtps
- 单点/随机 Set：200wtps

PedroDB 的适用场景：

- 以单点 Get，Set，多点 Set 为主的场景
- Value 的大小不超过 64 MiB
- 不需要范围扫描
- 对单点读取性能有较高要求
## 例子：使用 PerdoDB
### 基本接口
PedroDB 使用 LevelDB-like 的接口，并在 BitCask 存储模型的语义下，提供了一系列基本操作。所有操作都是线程安全的，可以多线程地执行。
```cpp
struct DB : pedrolib::noncopyable, pedrolib::nonmovable {
  static Status Open(const Options& options, const std::string& name,
                     std::shared_ptr<DB>* db);

  DB() = default;
  virtual ~DB() = default;

  virtual Status Get(const ReadOptions& options, std::string_view key,
                     std::string* value) = 0;

  virtual Status Put(const WriteOptions& options, std::string_view key,
                     std::string_view value) = 0;

  virtual Status Delete(const WriteOptions& options, std::string_view key) = 0;

  virtual Status Flush() = 0;

  virtual Status GetIterator(EntryIterator::Ptr*) = 0;

  virtual Status Compact() = 0;
};
```
### 打开/关闭数据库
打开数据库通过 `DB::Open` 函数，其中 `Options` 中可以设置一些数据库的配置，比如读缓存的大小，压实触发的时机，数据是否压缩等。当 `std::shared_ptr<DB>` 的引用计数为 0 时，数据库将会自动关闭。
```cpp
Options options;
std::shared_ptr<DB> db;

Status status = DB::Open(options, "test.db", &db);
if (status != Status::kOk) {
    std::cerr << "failed to open database" << std::endl;
}
```
### 写入或删除内容
写入（包括删除）默认时异步落盘，可以使用 `WriteOptions` 控制落盘的同步行为。也可以利用 `DB::Flush` 函数，批量将之前写入的内容落盘。与此同时，数据库默认会定期触发落盘，具体可见 `Options` 配置。
```cpp
WriteOptions options;
options.sync = false;	// 默认为异步落盘（同 RocksDB）

Status status;
status = db->Put(option, "hello", "world");
if (status != Status::kOk) {
    std::cerr << "failed to put key" << std::endl;
}

status = db->Delete(option, "foo");
if (status == Status::kNotExist) {
    std::cerr << "key is not exist" << std::endl;
}

// 将所有内容刷盘
db->Flush();
```
### 读取或扫描内容
BitCask 模型支持单点读，但不支持范围扫描。因此，使用 `DB::GetIterator` 的结果可能会乱序，迭代器将有可能无法读到在扫描过程中新增或删除的内容。
```cpp
ReadOptions options;
Status status;

std::string value;
status = db->Get(options, "hello", &value);
if (status != Status::kOk) {
    std::cerr << "failed to get key" << std::endl;
} else {
	std::cout << "value is: << value << std::endl;
}

EntryIterator::Ptr iterator;
status = db->GetIterator(&iterator);
if (status != Status::kOk) {
    std::cerr << failed to get iterator" << std::endl;
}

while (iterator->Valid()) {
	auto next = iterator->Next();
    std::cout << "key is: << next.key << ", value is: " << next.value << std::endl;
}
```
### 主动或被动压实
压实（Compact）是指删除文件中的无用内容，并读取有效内容压实成新文件的过程。压实可见显著减少数据库使用的磁盘空间，并提升读性能。但频繁的压实会对数据库性能产生影响，并造成过高的写放大。
PedroDB 允许用户设置主动压实和被动压实，以灵活地调节压实的时机。在被动压实中，PedroDB 采用基于阈值的压实，用户可以设置一定的时间间隔和触发阈值：

- 距离上次压实的时间不够长，不会触发压实
- 文件的空闲大小不够高，不会触发压实

默认情况下，触发压实的条件是：文件空闲空间占文件总空间超过 3/4。在这个条件下，数据库放大系数为：

- 默认写放大：71 %
- 默认空间放大：50 %
```cpp
Options options;
// 被动压实的触发阈值
options.compaction.threshold_bytes = kMaxFileBytes * 0.75;
// 压实的批量大小，用于平衡速度和对系统影响
options.compaction.batch_bytes = 4 * kMiB;

std::shared_ptr<DB> db;
DB::Open(options, "test.db", &db);

// 主动触发压实
db->Compact();
```
## 设计与实现
### 数据格式
数据格式分为数据内存布局和磁盘文件格式两种。本篇主要讲 PedroDB 的磁盘文件格式。
#### 元数据文件
元数据文件主要存储 PedroDB 的元数据，比如数据库名，索引和数据文件和数据库一些基本的配置。它的文件名格式为 `{db_name}.db`。
元数据文件由两部分组成：

- 元数据文件头 Header
- 元数据变更日志 ChangeLog

其中 Header 存储数据库名，数据库配置这些不可变的信息。ChangeLog 存储配置的改变，数据文件、索引文件的改变信息。
ChangeLog 部分是日志格式的存储，是 Append-Only 的，这意味着当元数据文件的大小会随着时间，变得越来越大。PedroDB 后续将支持元数据的手动压实，以减少数据库文件的大小并提高加载速度。
#### 数据文件
PedroDB 使用 BitCask 文件格式存储数据，文件名格式为 `{db_name}.{file_id}.data` 。具体来说，一个数据库有多个数据文件。其中 `file_id` 最新的文件称为活动文件，其余的文件称为只读文件。
所有的数据文件都由数据项 Record 填充而成。Record 存储了具体的 KeyValue 对，其数据布局如下：

| 域 | 偏移量 |
| --- | --- |
| type | 0 |
| checksum | 1 |
| key_size | 5 |
| value_size | 9 |
| version | 13 |
| key_data | 17 |
| value_data | - |

为了更高的性能，所有数据文件在创建时就会分配 128 MiB 的大小，未访问的部分将使用 0 进行填充。
#### 索引文件
PedroDB 使用索引文件加快数据库**崩溃恢复**的过程，索引文件的文件名格式为 `{db_name}.{file_id}.index`，索引文件与数据文件共享同一个 `file_id` ，每个数据文件至多对应一个索引文件。
索引文件记录了数据文件中每个 Key 对应的 `Entry` 位置。在崩溃恢复阶段，数据库将索引文件加载入内存中，就可以完成内存索引的恢复。索引文件的格式如下：

| 域 | 偏移量 |
| --- | --- |
| type | 0 |
| key_size | 1 |
| offset | 5 |
| len | 9 |
| key_data | 13 |

### 文件 与 I/O
要实现高性能的 KeyValue 数据库，文件管理相当重要：

- 只读文件 与 活动文件具有不同的 I/O 模式，必须按照各自的使用模式，单独处理
- 对于数据库而言，必须控制打开的文件数，以维持稳定的服务
- 实验中间的文件管理层，有助于未来实现异步的文件 I/O (基于 `eventfd(2)` 或 `io_uring`)
#### 文件管理模块
文件管理模块是 PedroDB 的内部模块，这里我们阐述它的主要功能

- 原子地向活动文件写入一个记录
- 原子地添加或删除一个文件
- 将当前活动文件刷盘
- 控制打开的文件个数
- 缓存已经打开的文件，方便后续的读取
#### 只读文件
只读文件，顾名思义就是只读取不写入的文件。在 I/O 访问模式上属于随机访问，我们使用 `pread(2)` 的方式进行文件的读取。对于连续读取的访问模式（如迭代器访问），我们通过每次读 `kPageSize` 的大小隐藏寻址的延迟，并减少读放大，从而提升读速度。
#### 可读写文件
可读写文件由几个性质：

- 在尾部顺序写入，在首部随机读取的文件
- 文件大小固定为 128 MiB
- 整个数据库只有一个文件负责数据写入

因此，我们采用 `mmap` 进行数据的写入和读取。好处如下：

- 写入后无需刷盘 (no force)，读取方就可以读取这个数据
- `msync` 支持异步刷盘
- `madvice` 可以指定数据的读访问模式，提高性能
- 通过实现 Buffer 接口，位于内存的 Record 可以零拷贝地写入到 磁盘中
```cpp
class ReadWriteFile final : public ReadableFile,
                            public WritableFile,
                            noncopyable,
                            nonmovable {
 public:
  using Ptr = std::shared_ptr<ReadWriteFile>;

 private:
  File file_;
  char* data_{};
  size_t write_index_{};
  const size_t capacity_{};

  ...
};
```
其中，一些变量的含义为 ：

- `data_` 是 `mmap` 映射的一段内存，大小为 128 MiB，通过 `MAP_SHARED` 模式关联到文件上。
- `capacity_` 是 文件的大小，一般为 128 MiB。
- `write_index_` 是下一个 `Record` 追加的位置
### 读取、写入与删除数据
#### 读取数据
读取数据会由 `DBImpl::HandleGet` 方法执行。它的步骤有几步：

- 从内存索引中找到 `Record` 的位置信息 `record::Dir`
- 通过文件管理器，打开对应的文件
- 打开文件后，通过 `RecordIterator`，找到 `Record` 对应的位置
- 取出对应的值。如果启用了值压缩特性，那么会先进行解压缩。
```cpp
Status DBImpl::HandleGet(const ReadOptions& options, const std::string& key,
                         std::string* value) {

  record::Dir dir;
  {
    auto lock = AcquireLock();
    auto it = indices_.find(key);
    if (it == indices_.end()) {
      return Status::kNotFound;
    }
    dir = it->second;
  }

  ReadableFile::Ptr file;
  auto stat = file_manager_->AcquireDataFile(dir.loc.id, &file);
  if (stat != Status::kOk) {
    PEDRODB_ERROR("cannot get file {}", dir.loc.id);
    return stat;
  }

  RecordIterator iterator(file.get());
  iterator.Seek(dir.loc.offset);
  if (!iterator.Valid()) {
    return Status::kCorruption;
  }

  auto entry = iterator.Next();
  if (!entry.Validate()) {
    return Status::kCorruption;
  }
  
  if (options_.compress) {
    Uncompress(entry.value, value);
  } else {
    value->assign(entry.value);
  }
  return Status::kOk;
}
```
#### 写入与删除数据
写入和删除数据都由 `DBImpl::HandlePut` 方法进行处理，其中删除数据时 `value` 为空。

- 先构建内存中的 `Record` 对象，获取时间戳 `timestamp`（由于还没有实现多版本，此处暂时赋值为 0）
- 如果配置了压缩，则会使用 `snappy` 算法压缩数据
- 将 Record 对象原子写入活动文件中（使用 `mmap` 实现零拷贝）
   - 如果 `value` 为空，表示是一个删除标记
   - 否则是一个更新标记
   - 写入的提交点是落盘的一瞬间
- 更新内存索引，并更新 Compact Hint，用于后续压实
- 如果设置了 `WriteOptions::sync`，则需要同步刷盘
```cpp
Status DBImpl::HandlePut(const WriteOptions& options, const std::string& key,
                         std::string_view value) {
  record::Entry<> entry;
  entry.type = value.empty() ? record::Type::kDelete : record::Type::kSet;
  entry.key = key;
  if (options_.compress) {
    Compress(value, &entry.value);
  } else {
    entry.value = value;
  }
  entry.checksum = record::Entry<>::Checksum(entry.key, entry.value);

  if (entry.SizeOf() > kMaxFileBytes) {
    PEDRODB_ERROR("key or value is too big");
    return Status::kNotSupported;
  }

  uint32_t timestamp = 0;
  entry.timestamp = timestamp;

  record::Location loc{};
  auto status = file_manager_->WriteActiveFile(entry, &loc);
  if (status != Status::kOk) {
    return status;
  }

  auto lock = AcquireLock();
  auto it = indices_.find(key);

  // only for insert.
  if (it == indices_.end()) {
    // invalid deletion.
    if (value.empty()) {
      UpdateUnused(loc, entry.SizeOf());
      return Status::kNotFound;
    }

    // insert.
    auto& dir = indices_[key];
    dir.loc = loc;
    dir.entry_size = entry.SizeOf();

    lock.unlock();
    if (options.sync) {
      file_manager_->Sync();
    }
    return Status::kOk;
  }

  // replace or delete.
  auto dir = it->second;
  UpdateUnused(dir.loc, dir.entry_size);

  if (value.empty()) {
    // delete.
    indices_.erase(it);
  } else {
    // replace.
    it->second.loc = loc;
    it->second.entry_size = entry.SizeOf();
  }

  lock.unlock();
  if (options.sync) {
    file_manager_->Sync();
  }
  return Status::kOk;
}
```
### 数据压实
#### 基于阈值的压实
PedroDB 利用 Compact Hint 收集每个文件与压缩相关的信息，其中包含了文件的空闲大小和压实状态。当文件的空闲大小超过阈值，并且文件压实状态不为 `kSchedule` 和 `kCompacting`，就会在空闲时调度压实任务，完成相应文件的压实工作。
#### 批量压实
压实是一个非常耗费资源的定时任务，因此 PedroDB 需要将压实的粒度从文件降低到一批 Record，以平衡压实速度和资源开销。
压实的批量由 `Options::compaction.batch_bytes` 配置。在批量压实时，`PedroDB` 将会读取文件 `batch_bytes` 大小的内容，挑选中其中有用的内容输出到活动文件中。
实践中，压实对读写吞吐量大约有 20-40% 左右的影响。对于负载随时间变化的应用，应选择低峰期进行压实，以提高高峰期数据库的读写性能。
### 崩溃恢复
崩溃恢复是数据库启动的第一个阶段。在关系型数据库中，崩溃恢复通常使用 ARIES 算法，回滚或重放日志，恢复内部的数据结构。对于 PedroDB 也是类似的。PedroDB 的崩溃恢复分为元数据恢复和索引恢复两步。
#### 元数据恢复
元数据恢复就是按照读取元数据文件，并使用元数据的 ChangeLog 恢复元数据的数据结构。
```cpp
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
```
#### 索引恢复
索引恢复就是通过索引文件或数据文件，恢复 PedroDB 内存索引的内容：

- 优先使用索引文件恢复
- 当使用索引文件恢复时，直接读取索引项恢复内存数据结构即可
- 当使用数据文件恢复时，需要从数据项构建索引项，然后恢复数据结构
```cpp
Status DBImpl::Recovery(file_id_t id) {
  ReadableFile::Ptr file;
  if (file_manager_->AcquireIndexFile(id, &file) == Status::kOk) {
    auto iter = IndexIterator(file.get());
    while (iter.Valid()) {
      Recovery(id, iter.Next());
    }
    return Status::kOk;
  }

  if (file_manager_->AcquireDataFile(id, &file) == Status::kOk) {
    auto iter = RecordIterator(file.get());
    while (iter.Valid()) {
      index::EntryView view;
      view.offset = iter.GetOffset();

      auto next = iter.Next();
      view.len = next.SizeOf();
      view.type = next.type;
      view.key = next.key;
      Recovery(id, view);
    }
    file_manager_->ReleaseDataFile(id);
    return Status::kOk;
  }

  return Status::kIOError;
}
```
#### 索引恢复算法
当恢复某一个具体的索引项时，需要按照以下的算法进行恢复：

- 当前的索引项为 `kSet`：
   - 当内存索引项不存在时，使用当前索引项 entry
   - 否则，选择较新的索引作为内存的索引项
   - 选择较旧的索引项来更新 Compact Hint
- 当前的索引项为 `kDelete`
   - 当内存索引项不存在时，或者内存索引项较新时
      - 跳过该索引项
      - 同时更新 Compact Hint，标记该位置为空闲
   - 如果内存的索引项较旧时
      - 删除内存索引项
      - 同时更新 Compact Hint，标记这两个位置都为空闲
```cpp
void DBImpl::Recovery(file_id_t id, index::EntryView entry) {
  record::Location loc(id, entry.offset);
  std::string key(entry.key);

  auto it = indices_.find(key);
  if (entry.type == record::Type::kSet) {
    if (it == indices_.end()) {
      auto& dir = indices_[key];
      dir.entry_size = entry.len;
      dir.loc = loc;
      return;
    }

    // indices has the newer version data.
    auto dir = it->second;
    if (dir.loc > loc) {
      UpdateUnused(loc, entry.len);
      return;
    }

    // never happen.
    if (dir.loc == loc) {
      PEDRODB_FATAL("meta.loc == loc should never happened");
    }

    // indices has the elder version data.
    UpdateUnused(dir.loc, dir.entry_size);

    // update indices.
    it->second.loc = loc;
    it->second.entry_size = entry.len;
  }

  // a tombstone of deletion.
  if (entry.type == record::Type::kDelete) {
    UpdateUnused(loc, entry.len);
    if (it == indices_.end()) {
      return;
    }

    // Recover(file_id_t) should be called monotonously,
    // therefore entry.loc is always monotonously increased.
    // should not delete the latest version data.
    auto& dir = it->second;
    if (dir.loc > loc) {
      return;
    }

    UpdateUnused(dir.loc, dir.entry_size);
    indices_.erase(it);
  }
}
```
## 实验
### 实验环境
| CPU | 13th Gen Inter(R) Core(TM) i9-13900HX 24c32t |
| --- | --- |
| Memory | Crucial Technology DDR5 5200Mhz 32GiB x2 |
| Kernel | 5.15.90.1-microsoft-standard-WSL2 |
| GCC | 11.3.0 |
| OS | Ubuntu 22.04.2 LTS |
| Storage | Samsung MZVL21T0HCLR-00BH1 1TiB SSD PCI-e x4.0 |

### 实验方法与对比对象
#### 对比对象

- **LevelDB：**A fast key-value storage library written at Google that provides an ordered mapping from string keys to string values. (version = 1.23)
- **RocksDB：**A library that provides an embeddable, persistent key-value store for fast storage. （version = 8.3.2）
#### 实验方法
测试使用 LevelDB 和 RocksDB 官方的 `db-bench` 程序，均使用默认参数，其中 key 大小为 16B，value 大小为 100B。LevelDB，RocksDB 和 PedroDB 均开启 snappy 压缩。随机读取和写入中，数据分布为均匀分布。
### 实验结果
下面是 PedroDB，LevelDB 和 RocksDB 在其中四个子项的测试结果、实验结果的数据单位均是操作每秒 (ops)

- PedroDB 的 Scan 不是顺序扫描，LevelDB 和 RocksDB 的 Scan 具有有序性，因此结果不具有可比性
- 在随机点查 (Point Get) 项目上，PedroDB 对比 LevelDB 和 RocksDB 具有一定的优势，很大程度基于几点
   - 随机读取的数据均匀分布，BlockCache 无法起到太大作用
   - PedroDB 基于 BitCask 模型，只需一次磁盘访问就可以获取值
   - LevelDB 和 RocksDB 基于 LSM-tree 模型，访问一个键可能需要多次磁盘访问
- 在随机插入（Fill Random）项目中，PedroDB 对比 LevelDB 和 RocksDB 也具有一定的优势
   - PedroDB 插入只需一次顺序的磁盘写入
   - LevelDB 和 RocksDB 在插入时，需要先插入到 memtable，并写 WAL。等到 memtable 满后，还需要将 memtable frozen，并写入磁盘，过程还可能触发 compaction，因此可能触发多次磁盘访问
   - LevelDB 和 RocksDB 支持 BatchWrite，理论上可以大大缓解上述问题，提高写速度。根据官方 benchmark，BatchWrite 可以带来 1.35x 的性能提升。
- 在大对象写入 (Fill 100k) 项目中，Value 的大小为 100,000B。PedroDB 的性能优于 LSM-tree 类的存储：
   - 写放大突出：大 Value 会导致 LSM-tree 模型的写放大现象变得严重，同时降低了有效写带宽
   - 数据仍然需要写到 memtable 和 WAL 中。WAL 加剧了写放大，而 memtable 使得数据不能“零拷贝”落盘
   - 压实占用了一定的磁盘带宽：在 LSM-tree 模型中，某一层的文件达到某个阈值后会触发压实，压实占用了一定的磁盘带宽
   - PedroDB 针对上述问题有一定优势：
      - 使用 BitCask 模型，数据顺序写入到磁盘中，能充分利用磁盘带宽。
      - PedroDB 使用了零拷贝技术，编码过程中 Entry 可以直接落盘，不需要写入内存。
      - PedroDB 使用了基于阈值的压实，在达到相应的阈值后，不会立刻压实，而是把压实任务调度到合适时机运行，避免 Compaction 占用写带宽

|  | **PedroDB** | **LevelDB** | **RocksDB** |
| --- | --- | --- | --- |
| **Get Random** | 1,258,178 | 566,572 | 431,778 |
| **Scan** | 29,411,764 | 11,904,762 | 9,090,909 |
| **Fill Random** | 1,851,851 | 492,853 | 482,858 |
| **Fill 100k** | 94,339 | 1,471 | 17,857 |
