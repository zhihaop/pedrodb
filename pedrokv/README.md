# PedroKV：A fast and reliable key-value storage service using Bitcask Model

## 简介

PedroKV 是基于 PedroDB 和 Pedronet 的一个 KeyValue 简单的单机存储服务，它提供了客户端，服务端，支持基本的 Get，Set，Delete
操作，能够支持**单机百万级别**的异步读取和写入。

### PedroDB

PedroDB 是一个基于 BitCask 模型的高性能 KeyValue 存储系统，它支持 Get，Set，Delete 和 Scan 操作，点查/写性能优于 LevelDB 和
RocksDB 等 LSM-tree 存储模型，能够达到**单机百万级**的随机查询和写入。

### Pedronet

Perdonet 是一个高性能的事件驱动网络编程框架，是 PedroKV 进行网络通信的基础。在 PingPong 1K
场景下，它的性能要略优于 `muduo`, `asio` 和 `netty` 框架，单核**每秒处理可超过 10w 个包**。

### 性能概要

key，value长度分别为 16，100（开启值压缩），分片数为 8，使用 Client 进行性能测试的结果如下（单位为每秒操作数 ops）

| 客户端数        | 1      | 4       | 16      | 64      |
|-------------|--------|---------|---------|---------|
| PedroKV 异步写 | 504286 | 1287001 | 1123595 | 1034661 |
| PedroKV 异步读 | 569638 | 1589825 | 1644736 | 1762114 |
| PedroKV 同步写 | 29034  | 125492  | 207232  | 674308  |
| PedroKV 同步读 | 28566  | 126758  | 230414  | 968523  |
| Redis 同步写   | 30652  | 186279  | 271788  | 420875  |
| Redis 同步读   | 29966  | 186332  | 285417  | 470256  |

## 例子：PedroKV 的使用

### PedroKV 服务端

```cpp
auto address = InetAddress::Create("0.0.0.0", 1082);
ServerOptions options;
options.db_path = "/tmp/test.db";
options.db_shards = 8;

auto server = pedrokv::Server(address, options);
server.Bind();
server.Start();

EventLoopGroup::Joins(options.worker_group, options.boss_group);
```

### 使用 PedroKV 客户端

#### 同步接口

```cpp
ClientOptions options;
SyncClient client(options, InetAddress::Create("127.0.0.1", 1082));

Response response = client.Get("hello");
if (response.type != ResponseType::kOk) {
    std::cerr << "failed to get key" << std::endl;
} else {
	std::cout << "key=" << response.data << std::endl;
}

response = client.Put("hello", "world");
if (response.type != ResponseType::kOk) {
    std::cerr << "failed to put key, reason: " << response.data << std::endl;
}

response = client.Delete("hello");
if (response.type != ResponseType::kOk) {
    std::cerr << "failed to delete key, reason: " << response.data << std::endl;
}
```

#### 异步接口

```cpp
ClientOptions options;
options.max_inflight = 1024;	// 当这个值设置为 1，异步接口将退化为同步接口

Client client(options, InetAddress::Create("127.0.0.1", 1082));

client.Get("hello", [](auto&& response) {
    if (response.type != ResponseType::kOk) {
        std::cerr << "failed to get key" << std::endl;
    } else {
    	std::cout << "key=" << response.data << std::endl;
    }
});

client.Put("hello", "world", [](auto&& response) {
    if (response.type != ResponseType::kOk) {
        std::cerr << "failed to put key, reason: " << response.data << std::endl;
    }
});

client.Delete("hello", [](auto&& response) {
    if (response.type != ResponseType::kOk) {
        std::cerr << "failed to delete key, reason: " << response.data << std::endl;
    }
});
```

## 设计与实现

### 数据协议

#### 请求 Request

请求 Request 是一个客户端发放服务端的消息，目前有三种类型 Get，Put，Delete。Pack 方法将一个 Request 变成 二进制流，UnPack
将二进制流 变成 Request 对象。

```cpp
enum class RequestType {
  kGet = 1,
  kPut = 2,
  kDelete = 3,
};

template <typename Key = std::string, typename Value = std::string>
struct Request {

  RequestType type;
  uint32_t id;
  Key key;
  Value value;

  template<typename ReadableBuffer>
  bool UnPack(ReadableBuffer*);

  template<typename WritableBuffer>
  void Pack(WritableBuffer*) const;
};
```

Request 的二进制格式如下：

| Field          | Offset |
|----------------|--------|
| content length | 0      |
| key length     | 2      |
| type           | 3      |
| id             | 4      |
| key            | 8      |
| value          | -      |

#### 响应 Response

响应 Response 是一个从服务端发给客户端的消息，目前有两种类型 Ok，Error。如果类型为 Error，那么 data 返回的是对应的错误信息。Pack
方法将一个 Response 变成 二进制流，UnPack 将二进制流 变成 Response 对象。

```cpp
enum class ResponseType { kOk, kError };

template <typename Value = std::string>
struct Response {
  ResponseType type{};
  uint32_t id{};
  Value data;

  template<typename WritableBuffer>
  void Pack(WritableBuffer* buffer) const;

  template<typename ReadableBuffer>
  bool UnPack(ReadableBuffer* buffer);
};
```

Response 的二进制格式如下：

| Field          | Offset |
|----------------|--------|
| content length | 0      |
| type           | 2      |
| id             | 3      |
| data           | 7      |

### 分片存储

PedroDB 实现了一个分片存储组件，它将数据按照 `key` 分成了多个分区，每个 `DB` 实例负责不同的分区，从而实现提升存储的可拓展性和性能。

```cpp
class SegmentDB : public DB {
  std::vector<std::shared_ptr<DBImpl>> segments_;
  std::shared_ptr<Executor> executor_;

 public:
  explicit SegmentDB(size_t n) : segments_(n) {}

  ~SegmentDB() override = default;

  static Status Open(const Options& options, const std::string& path, size_t n,
                     std::shared_ptr<DB>* db);

  DBImpl* GetDB(size_t h) { return segments_[h % segments_.size()].get(); }

  Status Get(const ReadOptions& options, std::string_view key,
             std::string* value) override;

  Status Put(const WriteOptions& options, std::string_view key,
             std::string_view value) override;

  Status Delete(const WriteOptions& options, std::string_view key) override;

  Status Flush() override;

  Status Compact() override;

  Status GetIterator(EntryIterator::Ptr* ptr) override;
};
```

PedroKV 使用了 `SegmentDB` 作为底层的存储。可以在 `ServerOptions` 中配置每个 `DB` 分片的配置和分片数。如果使用机械硬盘
HDD，建议将 `db_shards` 值设置为 1 以提高性能。

```cpp
struct ServerOptions {
  std::shared_ptr<EventLoopGroup> boss_group = EventLoopGroup::Create(1);
  std::shared_ptr<EventLoopGroup> worker_group = EventLoopGroup::Create();
  
  size_t db_shards = 8;
  pedrodb::Options db_options;
  std::string db_path;
};
```

### 服务端

#### 基本接口

PedroKV 借助 `pedronet::TcpServer` 实现了一个 TCP 服务器。具体方法如下，其中 `HandleRequest` 处理来自客户端的请求。

```cpp
class Server : nonmovable,
               noncopyable,
               public std::enable_shared_from_this<Server> {
  pedronet::TcpServer server_;
  pedronet::InetAddress address_;
  ServerOptions options_;

  std::shared_ptr<pedrodb::DB> db_;
  ServerCodec codec_;

  void HandleRequest(const TcpConnectionPtr& conn, const ResponseSender& sender,
                     const RequestView& requests);

 public:
  Server(pedronet::InetAddress address, ServerOptions options);

  void Bind() {
    server_.Bind(address_);
    PEDROKV_INFO("server bind success: {}", address_);
  }

  void Start() {
    server_.Start();
    PEDROKV_INFO("server start");
  }

  void Close() {
    db_.reset();
    server_.Close();
  }
};
```

#### 初始化

服务端初始化时，就会打开 `SegmentDB`
，成功后就会设置相应的回调。当一条请求进来后，它会经过编解码器，编解码器成功处理消息后，通过 `codec_.OnMessage`
方法，将具体的请求由 `HandleRequest` 处理。

```cpp
Server::Server(pedronet::InetAddress address,
                        pedrokv::ServerOptions options)
    : address_(std::move(address)), options_(std::move(options)) {
  server_.SetGroup(options_.boss_group, options_.worker_group);

  auto stat = pedrodb::SegmentDB::Open(options_.db_options, options_.db_path,
                                       options_.db_shards, &db_);
  if (stat != pedrodb::Status::kOk) {
    PEDROKV_FATAL("failed to open db {}", options_.db_path);
  }

  codec_.OnMessage([this](auto&& conn, auto&& sender, auto&& requests) {
    HandleRequest(conn, sender, requests);
  });

  server_.OnConnect(codec_.GetOnConnect());
  server_.OnClose(codec_.GetOnClose());
  server_.OnMessage(codec_.GetOnMessage());
}
```

#### 编解码器

当请求进来后，消息会传给编解码器的 `HandleMesage` 方法。`buffer` 入参存储了 Request 的二进制流。
在处理消息前，它会构建一个 `Sender` 对象，用于将上层的 `Response` 编解码成 二进制流，并发送出去。
在处理消息时，它会不断地尝试从二进制流中提取 `Request` 对象，直到不能 `UnPack` 就返回。
编解码器采用 `RequestView` 是 `Request<std::string_view, std::string_view>`的别名，通过使用 `RequestView` 作为 `Request`
可以降低过程中的不必要拷贝。

```cpp
void ServerCodec::HandleMessage(const TcpConnectionPtr& conn, ArrayBuffer* buffer) {
    ResponseSender sender([&](Response<>& response) {
        std::unique_lock lock{mu_};
        response.Pack(&output_);
    });

    while (true) {
        RequestView req;
        if (buffer_.ReadableBytes()) {
            if (req.UnPack(&buffer_)) {
                callback_(conn, sender, req);
                continue;
            }

            uint16_t len;
            if (!PeekInt(&buffer_, &len)) {
                buffer_.Append(buffer);
                continue;
            }

            len = std::min(len - buffer_.ReadableBytes(), buffer->ReadableBytes());
            buffer_.Append(buffer->ReadIndex(), len);
            buffer->Retrieve(len);
            continue;
        }

        if (req.UnPack(buffer)) {
            callback_(conn, sender, req);
            continue;
        }

        break;
    }

    std::unique_lock lock{mu_};
    if (output_.ReadableBytes()) {
        conn->Send(&output_);
    }
}
```

#### 处理请求

PedroKV 服务端通过 `HandleRequest`方法处理具体的请求。它会根据不同的请求类型，调用底层不同的 `SegmentDB`
的接口，并通过上文提到的 `ResponseSender` 来发送响应。

```cpp
void Server::HandleRequest(const TcpConnectionPtr&,
                           const ResponseSender& sender,
                           const RequestView& request) {

  Response response;
  response.id = request.id;
  pedrodb::Status status = pedrodb::Status::kOk;
  switch (request.type) {
    case RequestType::kGet: {
      status = db_->Get({}, request.key, &response.data);
      break;
    }
    case RequestType::kDelete: {
      status = db_->Delete({}, request.key);
      break;
    }
    case RequestType::kPut: {
      status = db_->Put({}, request.key, request.value);
      break;
    }
    default: {
      PEDROKV_WARN("invalid request receive, {}", (uint32_t)response.type);
      break;
    }
  }

  if (status != pedrodb::Status::kOk) {
    response.type = ResponseType::kError;
    response.data = fmt::format("err: {}", status);
  } else {
    response.type = ResponseType::kOk;
  }

  sender(response);
}
```

### 异步客户端

PedroKV 借助 `pedronet::TcpClient` 实现了一个异步 PedroKV 客户端。
它的主要方法有 `Get`，`Put`，`Delete`。当调用这三个接口时，对应的 `ResponseCallback` 会被调用有且只有一次。如果 `Get`
成功，那么就返回成功的 `Response`，否则返回失败的 `Response`。通过 `Response::type` 字段，我们可以轻松判断这个请求成功与否。
它最重要的内部方法是 `SendRequest` 和 `HandleResponse`，分别对应的请求的发送和处理。

```cpp
using ResponseCallback = std::function<void(Response<>)>;

class Client : nonmovable, noncopyable {

  ...
  void HandleResponse(std::queue<Response<>>& responses);
  void SendRequest(Request<> request, uint32_t id, ResponseCallback callback);
public:

  ...

  void Get(std::string_view key, ResponseCallback callback);

  void Put(std::string_view key, std::string_view value,
           ResponseCallback callback);

  void Delete(std::string_view key, ResponseCallback callback);

  void Start();
};
```

#### 初始化

PedroKV 客户端的初始化与服务端类似，也是设置各种回调，并于编解码器绑定在一起。当消息来临时，会以 `Buffer`
的形式传递到编解码器 `ClientCodec`，当编解码器处理完成后，将调用 `HandleResponse`出来所有相关的请求。

```cpp
codec_.OnMessage([this](auto& conn, auto& responses) { HandleResponse(responses); });

client_.OnError(error_callback_);
client_.OnMessage(codec_.GetOnMessage());
client_.OnClose(codec_.GetOnClose());
client_.OnConnect(codec_.GetOnConnect());
client_.Start();
```

#### 编解码器

PedroKV 客户端的编解码器 `ClientCodec` 与服务端的类似，这里不再阐述。不过，`ClientCodec`
会将多个响应打包在一起，通过客户端的 `HandleResponse` 方法进行处理，以降低锁的开销。

#### 发送请求

`Get`，`Put`，`Delete`方法最终会调用 `SendRequest` 方法发送请求。该方法的步骤如下：

- L4：如果当前 `in_flight` 请求数量大于 `max_inflight`，需要进行等待。
- L8：如果当前的请求 `id` 已经被占用了，返回错误。
- L16：设置回调表
- L18：通过 `pedronet::TcpClient` 发送请求，如果发送失败直接返回

```cpp
void Client::SendRequest(Request<> request, uint32_t id, ResponseCallback callback) {
  std::unique_lock lock{mu_};

  while (responses_.size() > options_.max_inflight) {
    not_full_.wait(lock);
  }

  if (responses_.count(id)) {
    Response response;
    response.id = id;
    response.type = ResponseType::kError;
    callback(response);
    return;
  }
  
  responses_[id] = std::move(callback);

  if (!client_.Write(std::move(request))) {
    Response response;
    response.id = id;
    response.type = ResponseType::kError;
    
    responses_[id](response);
    responses_.erase(id);
  }
}
```

#### 接收响应

当服务器发来响应时，`Client::HandleResponse`接口会处理响应的响应。`responses` 代表所有未处理的响应队列。
客户端将通过回调表 `Client::response_`，使用请求 `id` 来找到相应的回调函数，进行相应的处理。
最后通过 `not_full_.notify_all` 方法告知 `Client::SendRequest` 线程，目前已经可以接受新请求了，可以不用继续等待。

```cpp
void Client::HandleResponse(std::queue<Response<>>& responses) {
  std::unique_lock lock{mu_};

  while (!responses.empty()) {
    auto response = std::move(responses.front());
    responses.pop();
    auto it = Client::HandleResponse.find(response.id);
    if (it == responses_.end()) {
      return;
    }

    auto callback = std::move(it->second);
    if (callback) {
      callback(std::move(response));
    }
    responses_.erase(it);
  }
  not_full_.notify_all();
}
```

#### 展望

目前缺少一个超时和重试的机制。考虑到 `inflight` 的请求数量不大于 `max_infight`，可以通过 `EventLoop::ScheduleEvery`
的方式定期扫描回调表 `Client::responses_`。通过对比时间戳，发现超时的回调，即时返回超时失败信息。

## 实验

### 实验环境

| Type    | Properties                                     |
|---------|------------------------------------------------|
| CPU     | 13th Gen Inter(R) Core(TM) i9-13900HX 24c32t   |
| Memory  | Crucial Technology DDR5 5200Mhz 32GiB x2       |
| Kernel  | 5.15.90.1-microsoft-standard-WSL2              |
| GCC     | 11.3.0                                         |
| OS      | Ubuntu 22.04.2 LTS                             |
| Storage | Samsung MZVL21T0HCLR-00BH1 1TiB SSD PCI-e x4.0 |

```shell
net.core.rmem_default = 212992
net.core.rmem_max = 212992
net.core.wmem_default = 212992
net.core.wmem_max = 212992
net.ipv4.tcp_adv_win_scale = 1
net.ipv4.tcp_moderate_rcvbuf = 1
net.ipv4.tcp_rmem = 4096        131072  6291456
net.ipv4.tcp_wmem = 4096        16384   4194304
net.ipv4.udp_rmem_min = 4096
net.ipv4.udp_wmem_min = 4096
net.sctp.sctp_rmem = 4096       865500  4194304
net.sctp.sctp_wmem = 4096       16384   4194304
vm.lowmem_reserve_ratio = 256   256     32      0       0
```

### 实验方法

测试分为四个子项，其单位都是每秒操作数 ops

- 异步写：测试异步写入的性能。计算方法为 总操作数 / 从测试开始到全部数据 Flush 到磁盘所需时间。
- 异步读：测试异步读取的性能。计算方法为 总操作数 / 从测试开始到成功获取全部数据所需时间。
- 同步写：测试写入性能：计算方法为 总操作数 / 全部数据成功插入所需时间。
- 同步读：测试读取性能：计算方法为 总操作数 / 全部数据成功读取所需时间。

我们将通过 PedroKV::Client 往 PedroKV::Server 插入 2,000,000 条数据。

- 其中 Key 和 Value 的长度分别为 16 和 100。
- PedroKV 支持分片以提高多线程性能，我们设置分片数为 8。
- PedroDB 作为存储端将使用默认配置并打开 `snappy` 压缩。

我们将利用 `Redis` 中的 `redis-benchmark` 工具，针对 SyncPut 和 SyncGet 子项进行性能对比。

- Redis 使用默认设置，未进行特别调优，版本为 7.0.12。
- 我们保证 `Redis` 在内存中的数据不会被交换。
- 其中 Key 和 Value 的大小与上面保持一致。

### 实验结果

从实验结果可以看到：

- 通过异步的方法写入/获取数据，要比同步方法快得多，这是因为 PedroKV 在内部会将多个异步指令进行 Batch 来执行，因此可以提高其运行效率
- 在同步模式下，Redis 的性能高于 PedroKV，但当线程数继续提升时，由于 Redis 命令是单线程运行的，而 PedroKV
  支持多线程同时执行命令，因此性能要高于 Redis。

| 客户端数        | 1      | 4       | 16      | 64      |
  |-------------|--------|---------|---------|---------|
| PedroKV 异步写 | 504286 | 1287001 | 1123595 | 1034661 |
| PedroKV 异步读 | 569638 | 1589825 | 1644736 | 1762114 |
| PedroKV 同步写 | 29034  | 125492  | 207232  | 674308  |
| PedroKV 同步读 | 28566  | 126758  | 230414  | 968523  |
| Redis 同步写   | 30652  | 186279  | 271788  | 420875  |
| Redis 同步读   | 29966  | 186332  | 285417  | 470256  |

