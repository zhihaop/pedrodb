# Pedronet 网络库

## 简介

### 为什么需要网络库

虽然 Sockets API 非常简单易懂，但正确地编写多线程的高性能网络应用程序却不简单。

- 为了处理 C10K 问题，须使用 I/O 多路复用技术，这使得应用程序在架构上应该面向事件。
- 为了高效地处理网络事件，并利用现代 CPU 的性能，须拥有多个事件循环，每个事件循环绑定在一个线程上，一个事件循环处理多个网络事件。
- 由于非阻塞 I/O 的性质，我们需要在用户态设置 Buffer，并防止上层应用收到不完整的消息。

因此，为了降低编写网络应用程序的复杂性，使用网络库进行网络编程是非常重要的。

### 设计目标

Pedronet 是我最近开发的一个网络库，它的设计目标是：

- 用户无需关注网络库底层等细节，只需处理对应的事件，就可以完成网络应用的编写
- 使用 `epoll(7)`, `poll(2)` 实现多路复用，不支持 `select(2)`
- 支持多线程 Reactor 和 单线程 Reactor 模式，不支持多进程 Reactor
- 支持使用进程池、线程池进行业务逻辑的处理
- 支持异步任务，支持定时器
- 不支持 UDP，只支持 TCP

### 设计概要

Pedronet 是一个使用 C++14 开发的基于事件驱动的网络框架，其中 pedro 源自希腊语 "η πέτρα"，意思是坚硬的磐石。在设计中，我们借鉴了
muduo 和 netty 两个网络框架，并基于自己的场景开发了 Pedronet。

#### 性能速览

我们采用 PingPong 作为测试方法。每次消息大小分别为 1K，64K，1M。可以发现，消息体大小为 64 K时，吞吐量最大。消息体大小为 1K
时，asio 取得最好的吞吐量。消息体大小为 64K 时，muduo 略占上风。当消息体大小为 1M 时，来自 Java 的 netty 框架吞吐量最高。
Pedronet 在这些场景都有相对较好的性能。更多实验数据请看最后一节。

|          | 1K (MiB/s) | 1K (Msg/s) | 64K (MiB/s) | 64K (Msg/s) | 1M (MiB/s) | 1M (Msg/s) |
|----------|------------|------------|-------------|-------------|------------|------------|
| pedronet | 3004       | 3034204    | 50102       | 819234      | 27064      | 363064     |
| asio     | 3010       | 3040264    | 48902       | 842876      | 22917      | 359641     |
| netty    | 2830       | 2858454    | 48368       | 776442      | 27816      | 364047     |
| muduo    | 2998       | 3028143    | 50925       | 826535      | 18544      | 297298     |

#### Reactor 模型

Reactor 模型是一种基于事件驱动的，高性能的网络编程模型。经过多年的发展，Reactor 被广泛地使用并存在多个不同的变种。
我们使用多线程 Reactor 模型，在这个模型中，有一个 Main Reactor，负责处理与新建连接相关的情况，有多个 Sub Reactor，负责处理读写
I/O 请求。
每个 Reactor 都与一个事件循环（称为 EventLoop）绑定在一起。事件循环是一个不停运行的函数。当一个或多个事件被触发时，线程被唤醒并在事件循环中处理所有活动事件。此外，每个
Reactor 还有一个多路复用器（称为 Selector），用于监听哪些事件被触发，并将活动事件标记出来，被事件循环处理。

#### 非阻塞 I/O

Pedronet 使用了非阻塞 I/O 进行网络通讯，这意味着 `accept(2)`, `read(2)`, `write(2)`, `send(2)`
等系统调用，当管道不可读或不可写时，将不会阻塞线程，并直接返回。与此同时产生了一些问题，当用户的请求或响应发送或接受到一半时，怎么处理剩余的部分？如何高效地利用内存，从而减少用户缓存区拷贝的开销？当管道发生错误时，如何通知应用程序，并将管道恢复到正常的状态？

#### 用户态缓存 Buffer

为了解决非阻塞 I/O 带来的问题，Pedronet 设计了用户态的
Buffer，用于暂存因管道不可读写而暂时无法发送或接收的内容。为了高效利用内存，我们采用了 `readv(2)` 和 `writev(2)`
系统调用，并利用 `alloca(3)`分配栈内存的方式，降低了系统调用和内存分配的开销。

#### 管道 Channel

在 Pedronet 中，管道是双工（可以发送和接受信息）的，可多路复用的对象。一个 TCP 连接是一个管道，一个 UDS （Unix Domain
Socket）是一个管道。EventChannel 是一个 `eventfd(2)` 形成的管道，它可以在事件发生时唤醒
Selector，利用这个特性可以支持第三方的事件。TimerChannel 时一个由 `timerfd(2)` 形成的管道，它可以在定时器超时时唤醒
Selector。因此通过管道的抽象，Pedronet 理论上可以支持多种网络层和机制。

#### 回调与事件驱动

Pedronet 使用事件驱动的方式进行网络应用编程。它与以往同步阻塞 I/O 的编程方式大不相同。在同步阻塞 I/O
中，控制流将被阻塞以等待数据到来，当数据准备好时才从控制流恢复，整个逻辑是线性的。使用事件驱动的方式编写应用程序时，开发者应习惯这样的一种范式：

- 当数据来到时，框架会通知我，我不需要等待或轮询获取数据
- 当我要发送数据时，框架会帮我做好，等数据发送完成后，框架会通知我
- 当错误来临时，框架会通过回调告知我

一种更友好的编程方式称为协程。在同步阻塞 I/O
中，控制流的阻塞同时伴随着线程的阻塞，线程的阻塞和唤醒往往带来很多不必要的开销。协程在理论上，将控制流的阻塞和线程的阻塞分离开，并且以一种事件驱动的方式，通过调度器调度其他协程运行。理论上说，有栈协程的效率往往低于回调，但因其友好的编程方式，调试友好的特性被广泛使用。Pedronet
未来可能会考虑将支持协程，并利用 c++23 的 std::executor 适配协程。

## 例子：Echo 服务器/客户端

我们以 Echo C/S 服务器的方式，展示 Pedronet 的使用方法。Echo 服务是最简单的 TCP 服务，当客户端回复 "hello"
给服务器时，服务器同样回复 "hello"。使用事件驱动的思想开发服务时，我们应注意三个半事件：

- 连接事件：包括连接成功，断开连接
- 错误事件：连接失败，读写失败等
- 可读事件：数据已经准备好，可以进行读取
- 写完成事件：算半个事件，表示数据已经发送到对端

### Echo Server

同样，我们只需要注意三个半事件，完整的代码如下。图中的 SetGroup 函数是为了设置 TcpServer 运行的事件循环组，目前我们还不需要关注这一点。

```cpp
auto boss_group = EventLoopGroup::Create(1);
auto worker_group = EventLoopGroup::Create();

TcpServer server;
server.SetGroup(boss_group, worker_group);

server.OnConnect([](const TcpConnectionPtr &conn) {
    PEDRONET_INFO("peer connect: {}", *conn);
});

server.OnClose([](const TcpConnectionPtr &conn) {
    PEDRONET_INFO("peer disconnect: {}", *conn);
});

server.OnError([](const TcpConnectionPtr &conn, Error what) {
    PEDRONET_WARN("peer {} error: {}", *conn, what);
});

server.OnMessage([=](const TcpConnectionPtr &conn, Buffer &buffer, Timestamp now) {
    // Echo to peer.
    conn->SendPackable(&buffer);
});

server.Bind(InetAddress::Create("0.0.0.0", 1082));
server.Start();
```

当然，如果你不关心所有连接事件和错误事件，Echo 服务器的最简写法如下。

```cpp
auto boss_group = EventLoopGroup::Create(1);
auto worker_group = EventLoopGroup::Create();

TcpServer server;
server.SetGroup(boss_group, worker_group);

server.OnMessage([=](const TcpConnectionPtr &conn, Buffer &buffer, Timestamp now) {
    // Echo to peer.
    conn->SendPackable(&buffer);
});

server.Bind(InetAddress::Create("0.0.0.0", 1082));
server.Start();
```

### Echo Client

对于客户端，同样需要关注三个半事件。在这个客户端中，当连接到服务器上时，客户端将主动给服务器打招呼 "hello"
，服务器收到客户端的消息后，原样转发到客户端。客户端又原样转发到服务端，如此往复。这种模式称为 Ping
Pong，是一种测试网络质量和网络框架性能的一种方法。最简写法如下：

```cpp
auto worker_group = EventLoopGroup::Create();
TcpClient client(InetAddress::Create("127.0.0.1", 1082));
client.SetGroup(worker_group);

client.OnConnect([](const TcpConnectionPtr &conn) { conn->SendPackable("hello"); });

client.OnMessage([&reporter](const TcpConnectionPtr &conn, Buffer &buffer, auto) {
    conn->SendPackable(&buffer);
});

client.Start();
```

### 关闭服务器或客户端

有些时候，我们需要客户端或服务器主动关闭连接，TcpConnection 提供了一系列函数用于关闭连接。这些函数线程安全，可以安全地在事件循环外调用。

```cpp
void TcpConnection::Close();			// 等时机合适关闭连接
void TcpConnection::Shutdown();			// 等待数据写完后关闭写端
void TcpConnection::ForceShutdown();	// 强制关闭写端，可能有剩余写数据未处理
void TcpConnection::ForceClose();		// 强制关闭连接，可能有剩余读写数据未处理
```

## Pedronet 设计与实现

Pedronet 的设计借鉴了 muduo 和 netty 中的设计理念和概念。下面浅谈一下 Pedronet 主要模块的设计与实现

### 管道

在 Pedronet 中，管道是一个可以读写，并且可以被多路复用的对象。`GetFile`用于获取底层的文件描述符，并将其与 `EventLoop`
和 `Selector` 绑定。`HandleEvents` 是一个回调，当 `Channel` 触发事件时，`EventLoop`
将会将触发的事件类型和当前时间通过 `HandleEvents` 接口告知 `Channel`。

```cpp
struct Channel : pedrolib::noncopyable, pedrolib::nonmovable {

  // For pedronet::Selector.
  virtual File &GetFile() noexcept = 0;
  [[nodiscard]] virtual const File &GetFile() const noexcept = 0;
  virtual void HandleEvents(ReceiveEvents events, Timestamp now) = 0;
  [[nodiscard]] virtual std::string String() const = 0;
  virtual ~Channel() = default;
};
```

目前 Pedronet 实现的 Channel 有三种：EventChannel，SocketChannel，TimerChannel。继承关系如下图：
![image.png](https://cdn.nlark.com/yuque/0/2023/png/28260923/1689240875684-d15e4bd4-b3be-423e-96f1-dbcfb5739904.png#averageHue=%23f5f5f5&clientId=u10919922-e1c1-4&from=paste&height=401&id=uba793add&originHeight=701&originWidth=1144&originalType=binary&ratio=1.75&rotation=0&showTitle=false&size=91502&status=done&style=none&taskId=uda7172a0-d877-4704-aa90-32c0788b7e4&title=&width=653.7142857142857)

### 多路复用器

`Selector` 是一个多路复用器，它可以注册多个 Channel，并在这些 Channel 状态改变时通知事件循环。底层实现上，它是对操作系统多路复用
API 的一种封装。`Selector`可以通过 `select(2)`，`poll(2)`，`epoll(7)`，`kqueue`实现。
![image.png](https://cdn.nlark.com/yuque/0/2023/png/28260923/1689247419760-9cd1d822-c117-4a8c-a6fe-732b0a2731b6.png#averageHue=%23f1f1f1&clientId=u10919922-e1c1-4&from=paste&height=169&id=u0945558d&originHeight=408&originWidth=736&originalType=binary&ratio=1.75&rotation=0&showTitle=false&size=49217&status=done&style=none&taskId=u26baa700-148b-4367-bd0c-0fdc2be16f5&title=&width=305.3999938964844)
Pedronet 默认只实现了 `EpollSelector`，如果需要支持 MacOS，还需要实现 `poll(2)` 或 `kqueue`。`EpollSelector` 的实现如下：

```cpp
class EpollSelector : public File, public Selector {
  std::vector<struct epoll_event> buf_;

  void internalUpdate(Channel *channel, int op, SelectEvents events);

public:
  EpollSelector();
  ~EpollSelector() override;
  void SetBufferSize(size_t size);

  void Add(Channel *channel, SelectEvents events) override;
  void Remove(Channel *channel) override;
  void Update(Channel *channel, SelectEvents events) override;

  Error Wait(Duration timeout, SelectChannels *selected) override;
};
```

#### 创建 EpollSelector

首先，EpollSelector 会创建一个 `EpollFile`，用于实现多路复用。`buf_`是暂存所有 epoll 事件的数组。

```cpp
inline static File CreateEpollFile() {
    int fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (fd <= 0) {
        PEDRONET_FATAL("failed to create epoll fd, errno[{}]", errno);
    }
    return File{fd};
}

EpollSelector::EpollSelector() : File(CreateEpollFile()), buf_(8192) {}
```

#### `epoll_ctl` 控制 `EpollFile`

```cpp
void EpollSelector::internalUpdate(Channel *channel, int op,
                                   SelectEvents events) {
  struct epoll_event ev {};
  ev.events = events.Value();
  ev.data.ptr = channel;

  int fd = channel->GetFile().Descriptor();
  if (::epoll_ctl(fd_, op, fd, op == EPOLL_CTL_DEL ? nullptr : &ev) < 0) {
    PEDRONET_FATAL("failed to call epoll_ctl, reason[{}]", errno);
  }
}
```

#### 注册/更新/注销 Channel

```cpp
void EpollSelector::Add(Channel *channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_ADD, events);
}

void EpollSelector::Update(Channel *channel, SelectEvents events) {
  internalUpdate(channel, EPOLL_CTL_MOD, events);
}

void EpollSelector::Remove(Channel *channel) {
  internalUpdate(channel, EPOLL_CTL_DEL, SelectEvents::kNoneEvent);
}
```

#### 等待读写事件

`EpollSelector::Wait`会阻塞一段时间 `timeout`，直到超时或绑定的 `Channel` 读写状态发生改变。被选中的 `Channel`
将会输出到 `SelectChannels`中

```cpp
Error EpollSelector::Wait(Duration timeout, SelectChannels *selected) {
  int n = ::epoll_wait(fd_, buf_.data(), buf_.size(), timeout.Milliseconds());
  selected->now = Timestamp::Now();
  selected->channels.clear();
  selected->events.clear();

  if (n == 0) {
    return Error::Success();
  }

  if (n < 0) {
    return Error{errno};
  }

  selected->channels.resize(n);
  selected->events.resize(n);
  for (int i = 0; i < n; ++i) {
    selected->channels[i] = static_cast<Channel *>(buf_[i].data.ptr);
    selected->events[i] = ReceiveEvents{buf_[i].events};
  }
  return Error::Success();
}
```

### 事件循环

`EventLoop` 是 Pedronet 网络库的核心，是事件驱动网络框架的重要组成部分。`EventLoop` 继承自 `Executor`，具有执行、定时和调度等重要功能。
![image.png](https://cdn.nlark.com/yuque/0/2023/png/28260923/1689247575939-c9afd4fc-5f14-402e-a227-a8ec4525d08e.png#averageHue=%23f4f4f4&clientId=u10919922-e1c1-4&from=paste&height=154&id=u9a407b17&originHeight=384&originWidth=1215&originalType=binary&ratio=1.75&rotation=0&showTitle=false&size=70416&status=done&style=none&taskId=u22951fd1-a80d-4085-bbb8-0db392b990e&title=&width=486)

#### Loop！让一起运转起来

`EventLoop` 最重要的就是事件循环。在事件循环内部，`EventLoop`
单线程地处理各个事件，循环往复。为了高效地监听和处理所有事件，我们统一使用了 `Selector`
对所有事件的消息进行监听，它不仅会监听网络消息，还会处理定时器超时，有新任务等事件。当事件被激活，我们就去处理它。

```cpp
void EventLoop::Loop() {
  PEDRONET_TRACE("EventLoop::Loop() running");

  // Bind offset thread to event loop.
  auto &offset = core::Thread::Current();
  offset.BindEventLoop(this);
	
  SelectChannels selected;
  while (state()) {
    // Wait for incoming events.
    auto err = selector_->Wait(kSelectTimeout, &selected);
    if (!err.Empty()) {
      PEDRONET_ERROR("failed to call selector_.Wait(): {}", err);
      continue;
    }

    // Process events.
    size_t n_events = selected.channels.size();
    for (size_t i = 0; i < n_events; ++i) {
      Channel *ch = selected.channels[i];
      ReceiveEvents event = selected.events[i];
      ch->HandleEvents(event, selected.now);
    }

    // Process callbacks.
    std::unique_lock<std::mutex> lock(mu_);
    std::swap(running_tasks_, pending_tasks_);
    lock.unlock();

    for (auto &task : running_tasks_) {
      task();
    }

    running_tasks_.clear();
  }

  offset.UnbindEventLoop(this);
}
```

#### 统一事件处理

上文我们提到，`Channel` 不仅可以是 `SocketChannel`，还可以是 `EventChannel` 和  `TimerChannel`。这两个 Channel
属于功能性的 `Channel` 。将一个第三方事件和 `EventChannel` 结合，就可以纳入多路复用的事件管理中。将一个 `TimerChannel`
和基于堆的定时器结合，就可以实现事件的调度和超时处理。
综上，Channel 是实现统一事件处理的关键。我们根据不同的需求，实现不同的 `Channel::HandleEvents`接口，就可以将多种事件纳入多路复用的事件驱动应用程序中。

#### 调度与超时

`EventLoop` 提供了四个调度相关的接口，他们可以：

- 稍后调度：将任务放入调度队列尾部
- 延时调度：将任务放入延时调度队列尾部
- 重复调度：将任务放入延时调度队列尾部，并隔一段时间重复调度该任务
- 取消调度：取消某个任务，不再调度

```cpp
void Schedule(Callback cb) override;
uint64_t ScheduleAfter(Duration delay, Callback cb) override;
uint64_t ScheduleEvery(Duration delay, Duration interval, Callback cb) override;
void ScheduleCancel(uint64_t id) override;
```

##### 调度队列与运行队列

调度队列存放了所有需要稍后执行的任务：

- 当任务需要被调度时，首先会被放入调度队列
- `EventLoop` 将会判断是否需要触发事件，唤醒 `Selector`
    - 如果之前调度队列内没有任务，需要唤醒 Selector
    - 如果之前调度队列内有任务，但没有任务处于运行状态，也需要唤醒 Selector
    - 唤醒的方式是对 `EventChannel` 进行写操作，使得 `EventChannel` 可读
- 当 `EventLoop` 线程被唤醒时，会先处理网络相关的事件
- 紧接着，将调度队列中的所有任务移动到运行队列
- `EventLoop` 执行运行队列中的所有任务

##### 基于堆的定时器

延迟调度队列是一个堆，存放了所有需要延迟执行的任务：

- 当任务需要被延时调度时，会先生成全局唯一的定时器 ID
    - 将定时器 ID 和 任务 作为 Key 和 Value 插入到回调表中
    - 将任务对应的弱回调插入到堆中。当任务被删除，弱回调将会失效
    - 更新 `TimerChannel` 的超时时间，使得下一个任务能够准时触发
- 当任务超时时，`EventLoop` 将会唤醒（因为 `TimerChannel` 变得可读）
- `TimerChannel::HandleEvents` 回调将会执行所有超时的任务
    - 如果任务需要循环执行，那么更新延时，进行下一次调度
    - 如果任务不需要循环执行，那么从回调表中删除任务
    - 最后更新 `TimerChannel` 的超时时间

##### 弱回调

弱回调提供了一种删除延时任务的方法。

- 任务被加入到回调表中，它的所有权语义属于回调表。
- 弱回调是任务的弱引用
- 当任务从回调表中移除，弱回调就会失效
- 实践上通过 std::weak_ptr 实现弱回调，性能比较高

### 事件循环组

`EventLoopGroup`是一个事件循环组，它由多个事件循环`EventLoop`构成，每个事件循环运行在一个独立线程中。当一个任务需要被调度时，将会使用
Round-Robin 的方式，从事件循环组中选取一个事件循环，由它来负责一个任务。

```cpp
class EventLoopGroup;
using EventLoopGroupPtr = std::shared_ptr<EventLoopGroup>;

class EventLoopGroup : public Executor {
  pedrolib::StaticVector<EventLoop> loops_;
  pedrolib::StaticVector<std::thread> threads_;
  std::atomic_size_t next_;
  size_t size_;

  size_t next() noexcept;

  void join();

public:
  explicit EventLoopGroup(size_t threads)
      : loops_(threads), threads_(threads), size_(threads), next_(0) {}

  template <typename Selector = EpollSelector>
  static EventLoopGroupPtr Create() {
    return Create<Selector>(std::thread::hardware_concurrency());
  }
  
  template <typename Selector = EpollSelector>
  static EventLoopGroupPtr Create(size_t threads) {
    auto group = std::make_shared<EventLoopGroup>(threads);

    for (size_t i = 0; i < threads; ++i) {
      auto selector = std::make_unique<Selector>();
      group->loops_.emplace_back(std::move(selector));
    }
    for (size_t i = 0; i < threads; ++i) {
      auto &loop = group->loops_[i];
      group->threads_.emplace_back([&loop] { loop.Loop(); });
    }
    return group;
  }

  ~EventLoopGroup() override { join(); }

  EventLoop &Next() { return loops_[next()]; }

  void Join() override;

  void Schedule(Callback cb) override { Next().Schedule(std::move(cb)); }

  uint64_t ScheduleAfter(Duration delay, Callback cb) override;

  uint64_t ScheduleEvery(Duration delay, Duration interval,
                         Callback cb) override;

  void ScheduleCancel(uint64_t id) override;

  void Close() override;
};
```

#### BossGroup

`BossGroup` 是负责接收连接的事件循环组。当连接到达时，`Acceptor` 将会调用 `accept(2)` 接受连接，并将其分配给 `WorkerGroup`
中的事件循环。
> 概念
> 从概念上讲，BossGroup 相当于 Reactor 模型中的 main Reactor，它的作用是：接收连接，并派发给连接给 sub
> Reactor，由他们来处理该连接的读写请求

#### WorkerGroup

`WorkerGroup` 是负责处理 I/O 事件的事件循环组。当 `Acceptor` 接收连接后，连接将会分配给 `WorkerGroup`
中的某个事件循环 `EventLoop`。此后所有与该连接相关的读写事件都由该`EventLoop`监听并实际执行。
> 概念
> 从概念上讲，WorkerGroup 相当于 Reactor 模型中的 sub Reactor，它的作用是：处理具体的读写请求。一般来说，WorkerGroup 不应该处理除
> I/O 以外的其他耗时任务，这些任务应该分配给单独的线程池执行，避免阻塞 EventLoop。

### 用户态缓冲区 Buffer

Buffer 是一个用户态缓冲区。它借鉴了 io.netty.ByteBuf 的设计，使用 `read_index` 和 `write_index` 来标记已经读写的位置，它的基本操作如下：

- `ReadIndex()`：获取 `read_index` 的值
- `WriteIndex()`：获取 `write_index` 的值
- `ReadableBytes()`：可以读取的字节数
- `WritableBytes()`：可以写入的字节数
- `Append(size_t n)`：将 `write_index` 往后移动，表示加入了`n`字节的数据
- `Retrieve(size_t n)`：将 `read_index` 往后移动，表示已经读取了`n`字节的数据
- `Reset()`：将 `read_index` 和 `write_index` 都移动到缓冲区头部
- `Append(source, ...)`：从 `source` 中读取数据，写入到 `Buffer` 中，最后移动`write_index`
- `Retrieve(target, ...)`：写入数据到 `target` 中，最后移动 `read_index`

```cpp
struct Buffer {
  [[nodiscard]] virtual size_t ReadableBytes() const noexcept = 0;
  [[nodiscard]] virtual size_t WritableBytes() const noexcept = 0;
  virtual void EnsureWritable(size_t) = 0;

  [[nodiscard]] virtual size_t Capacity() const noexcept = 0;
  virtual void Retrieve(size_t) = 0;
  virtual void Append(size_t) = 0;
  virtual void Reset() = 0;

  // +-------------------+------------------+------------------+
  // | discardable bytes |  readable bytes  |  writable bytes  |
  // +-------------------+------------------+------------------+
  // |                   |                  |                  |
  // 0      <=      readerIndex   <=   writerIndex    <=    capacity
  virtual const char *ReadIndex() = 0;
  virtual char *WriteIndex() = 0;

  virtual size_t Append(const char *data, size_t n) = 0;
  virtual size_t Retrieve(char *data, size_t n) = 0;
  virtual ssize_t Append(File *source) = 0;
  virtual ssize_t Retrieve(File *target) = 0;
  virtual size_t Append(Buffer *buffer) = 0;
  virtual size_t Retrieve(Buffer *buffer) = 0;

  template <typename Int> void RetrieveInt(Int *value) {
    Retrieve(reinterpret_cast<char *>(value), sizeof(Int));
    *value = betoh(*value);
  }

  template <typename Int> void AppendInt(Int value) {
    value = htobe(value);
    Append(reinterpret_cast<const char *>(&value), sizeof(Int));
  }
};
```

Buffer 的实现有 ArrayBuffer，BufferSlice，BufferView（deprecated）和 CompositeBuffer (todo)，他们的继承关系如下。
![image.png](https://cdn.nlark.com/yuque/0/2023/png/28260923/1689304984115-9e23f0a6-6685-4cb6-9de4-d6d0f8121cb5.png#averageHue=%23f2f2f2&clientId=u9146c097-e4b5-4&from=paste&height=110&id=u487cba0d&originHeight=275&originWidth=1062&originalType=binary&ratio=2.5&rotation=0&showTitle=false&size=48248&status=done&style=none&taskId=uc4a74fb9-b73a-4365-a38f-738b6c907e4&title=&width=424.8)

#### ArrayBuffer

ArrayBuffer 本质是一个 `std::vector`构成的缓存区。它基本的数据结构如下：

```cpp
class ArrayBuffer final : public Buffer {
  static const size_t kInitialSize = 1024;

  std::vector<char> buf_;
  size_t read_index_{};
  size_t file_index_{};

  ...
};
```

当空间不足时，它首先将会尝试删除 `DiscardableBytes`，如果空间仍然不够，将会把容量调整为原有的两倍

```cpp
void ArrayBuffer::EnsureWritable(size_t n) {
  size_t w = WritableBytes();
  if (n <= w) {
    return;
  }

  if (read_index_ + w > n) {
    size_t r = ReadableBytes();
    std::copy(buf_.data() + read_index_, buf_.data() + file_index_,
              buf_.data());
    read_index_ = 0;
    file_index_ = read_index_ + r;
    return;
  }
  size_t delta = n - w;
  size_t size = buf_.size() + delta;
  buf_.resize(size << 1);
}
```

在接收来自 Socket 的数据时，会优先使用可写缓存区，若空间还是不够将会使用栈缓存区，并触发扩容。

```cpp
ssize_t ArrayBuffer::Append(File *source) {
  char buf[65535];
  size_t writable = WritableBytes();
  std::string_view views[2] = {{buf_.data() + file_index_, writable},
                               {buf, sizeof(buf)}};

  const int cnt = (writable < sizeof(buf)) ? 2 : 1;
  ssize_t r = source->Readv(views, cnt);
  if (r <= 0) {
    return r;
  }

  if (r <= writable) {
    Append(r);
    return r;
  }

  EnsureWritable(r);
  Append(writable);
  Append(buf, r - writable);
  return r;
}
```

#### BufferSlice

BufferSlice 是一块连续内存空间的引用，它只是将这块内存包装为一个 Buffer，供应用层使用。

#### CompositeBuffer

CompositeBuffer 是多个 Buffer 对象的集合。它本质上参考了 io.netty.buffer.CompositeByteBuf 的设计。当我们需要在 ArrayBuffer
中添加更多内容时，可能会触发扩容，导致不必要的内存分配和拷贝，`CompositeBuffer`则可以解决这个问题，实现 Buffer 侧的 零拷贝。
当这块 Buffer 需要被写入 Socket 时，需要使用 `writev(2)` 系统调用，它可以同时读取多块连续内存空间，并把他们写入 Socket 中。

### TCP 连接

TCP 连接是一个 `SocketChannel`，它绑定到某个 `EventLoop` 上，由该 `EventLoop` 线程负责它的所有读写。`TcpConnection`
也是一个事件驱动思想的组件，因此要使用它，我们需要处理至少三个半事件：

- 连接事件 `OnConnect`/`OnClose`：处理 TcpConnection 的连接和断开
- 错误事件 `OnError`：处理网络 I/O 的错误
- 可读事件 `OnMessage`：有消息到来，需要读取
- 写完成事件 `OnWriteComplete`：之前的写入已经完成

#### TcpConnection 的状态

在 Pedronet 中，TCP 连接具有四个状态

- `kConnecting`：在用户处理连接事件前，TCP 连接的状态都是 `kConnecting`
- `kConnected`：用户处理连接事件后，TCP 连接 将会和 `EventLoop` 绑定，并在 `Selector` 中注册。现在，当 TCP
  连接可读或发生错误时，就会触发相应的事件，并回调相应的接口。
- `kDisconnecting`：当用户主动调用 `Shutdown` 或 `Close` 接口时，TCP 连接将会进入 `kDisconnecting`
  状态。此时，所有 `TcpConnection::SendPackable` 操作都不能进行，TCP
  连接将会将未发送的数据发送完成后，关闭写端或关闭连接，进入 `kDisconnected` 状态
- `kDisconnected`：此时，TCP 连接 将会和 `EventLoop` 解绑，并从 `Selector` 中注销。情况有四种
    - 对端主动关闭写端，并且对端的数据处理完成，TCP 连接将会关闭
    - 对端重置了连接，TCP 连接会直接关闭
    - TCP 连接处于 `kDisconnecting` 状态中，并且完成了剩余数据的发送或处理
    - 主动调用了 `TcpConnection::ForceShutdown` 或 `TcpConnection::ForceClose`

#### 内部状态

TcpConnection 在内部维护了若干状态：

- `*_callback_`表示给上层应用的回调。
- `ctx_` 是当前 TcpConnection 的上下文，由应用程序保存
- `output_` 和 `input_` 是 TcpConnection 的输出/输入缓存区
- `channel_` 是 TcpConnection 对应的 SocketChannel
- `local_` 和 `peer_` 是本地和对端的地址
- `eventloop_`  表示 TcpConnection 当前运行的 `EventLoop`

```cpp
class TcpConnection : pedrolib::noncopyable,
                      pedrolib::nonmovable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
  enum class State { kConnected, kDisconnected, kConnecting, kDisconnecting };

protected:
  std::atomic<State> state_{TcpConnection::State::kConnecting};

  MessageCallback message_callback_{};
  WriteCompleteCallback write_complete_callback_{};
  HighWatermarkCallback high_watermark_callback_{};
  ErrorCallback error_callback_;
  CloseCallback close_callback_;
  ConnectionCallback connection_callback_{};
  std::any ctx_{};

  ArrayBuffer output_;
  ArrayBuffer input_;

  SocketChannel channel_;
  InetAddress local_;
  InetAddress peer_;
  EventLoop &eventloop_;
  ...
};
```

#### 可读事件

当 TCP 连接收到消息时，就会触发可读事件，`EventLoop` 就会调用 `TcpConnection::handleRead` 接口，从 `SocketChannel`
中获取数据，并填充到输入缓存区中，最后通过 `message_callback_` 回调告知用户有消息到达。读取到的字节为
0，代表对端已经关闭写端，不会再有数据过来。当处理完所有数据时，就会关闭连接。

```cpp
void TcpConnection::handleRead(Timestamp now) {
  ssize_t n = input_.Append(&channel_.GetFile());
  PEDRONET_TRACE("read {} bytes", n);
  if (n < 0) {
    PEDRONET_ERROR("failed to read buffer");
    handleError(channel_.GetError());
    return;
  }

  if (n == 0) {
    PEDRONET_INFO("close because no data");
    handleClose();
    return;
  }

  if (message_callback_) {
    message_callback_(shared_from_this(), input_, now);
  }
}
```

#### 发送数据

用户通过 `TcpConnection::SendPackable` 接口向对端发送数据。为了保证线程安全，如果调用者在 EventLoop
线程内，可以安全地发送数据，否则需要将发送请求调度到 `EventLoop` 内部再进行发送。

```cpp
template <class BufferPtr> void SendPackable(BufferPtr buffer) {
    if (eventloop_.CheckUnderLoop()) {
        handleSend(buffer.get());
        return;
    }

    eventloop_.Schedule(
        [this, buf = std::move(buffer)]() mutable { handleSend(buf.get()); });
}
```

在 `EventLoop` 内部发送数据时，会先尽力而为地发送 Buffer 中的数据。剩余的因窗口大小或其他原因无法发送的数据将会保存在用户态缓存区
Buffer 中，当 TCP 连接可写时再次尝试发送数据。

```cpp
void TcpConnection::handleSend(Buffer *buffer) {
  eventloop_.AssertUnderLoop();

  State s = state_;
  if (s != State::kConnected) {
    PEDRONET_WARN("{}::SendPackable(): give up sending buffer", *this);
    return;
  }

  if (trySendingDirect(buffer) < 0) {
    auto err = channel_.GetError();
    if (err.GetCode() != EWOULDBLOCK) {
      handleError(err);
      return;
    }
  }

  size_t w = output_.WritableBytes();
  size_t r = buffer->ReadableBytes();
  if (w < r) {
    output_.EnsureWritable(r);
    if (high_watermark_callback_) {
      high_watermark_callback_(shared_from_this(), r - w);
    }
  }

  if (output_.Append(buffer) > 0) {
    channel_.SetWritable(true);
  }
}
```

当输出缓存区有数据，并且 TCP 连接可写时，`EventLoop` 将会主动调用 `TcpConnection::handleWrite`
触发数据的发送。当所有数据发送完成时，将会触发写完成事件的回调。特别地，如果 TCP 连接的状态是 `kDisconnecting`
，说明调用了 `Shutdown` 或 `Close`  接口，并完成了剩余数据的发送工作，这个时候可以关闭 `SocketChannel` 的写端，准备关闭连接。

```cpp
void TcpConnection::handleWrite() {
  if (!channel_.Writable()) {
    PEDRONET_TRACE("{} is down, no more writing", *this);
    return;
  }

  ssize_t n = output_.Retrieve(&channel_.GetFile());
  if (n < 0) {
    handleError(channel_.GetError());
    return;
  }

  if (output_.ReadableBytes() == 0) {
    channel_.SetWritable(false);
    if (write_complete_callback_) {
      eventloop_.Run([connection = shared_from_this()] {
        connection->write_complete_callback_(connection);
      });
    }

    if (state_ == State::kDisconnecting) {
      channel_.CloseWrite();
    }
  }
}
```

#### TCP 连接的关闭

有些时候，我们需要客户端或服务器主动关闭连接，TcpConnection 提供了一系列函数用于关闭连接。这些函数线程安全，可以安全地在事件循环外调用。

```cpp
void TcpConnection::Close();			// 等时机合适关闭连接
void TcpConnection::Shutdown();			// 等待数据写完后关闭写端
void TcpConnection::ForceShutdown();	// 强制关闭写端，可能有剩余写数据未处理
void TcpConnection::ForceClose();		// 强制关闭连接，可能有剩余读写数据未处理
```

以`TcpConnection::Shutdown` 为例：

- 它会将 TCP 连接的状态设置为 `kDisconnecting`，后续对 `TcpConnection::SendPackable` 的操作将被丢弃
- 如果没有尚未处理的数据，就关闭写端。反之，就继续处理仍在缓冲区中的请求
- 当缓冲的数据发送完毕时（见发送数据一章），就会触发连接的关闭
    - 将 TCP 连接的状态设置为 `kDisconnected`
    - 从 `EventLoop` 和 `Selector` 中注销 `Channel`
- 当连接最终被关闭时，将会触发 `close_callback_`
- 最后，在 `TcpConnection` 对象析构时，文件描述符被关闭，所有与之相关的资源将被释放

```cpp
void TcpConnection::Shutdown() {
  State s = State::kConnected;
  if (!state_.compare_exchange_strong(s, State::kDisconnecting)) {
    return;
  }

  eventloop_.Run([this] {
    if (output_.ReadableBytes() == 0) {
      PEDRONET_TRACE("{}::Close()", *this);
      channel_.SetWritable(false);
      channel_.CloseWrite();
    }
  });
}

void TcpConnection::handleClose() {
  if (state_ == State::kDisconnected) {
    return;
  }
  PEDRONET_INFO("{}::handleClose()", *this);
  state_ = State::kDisconnected;
  eventloop_.Deregister(&channel_);
}
```

### TcpServer 服务器

TcpServer 表示了一个 TCP 服务器，它封装了 `accept(2)`，`listen(2)` 等众多方法，并实现了多线程 Reactor
模型。同样地，它也是一个事件驱动思想的组件，因此要使用它，我们需要处理至少三个半事件：

- 连接事件 `OnConnect`/`OnClose`：处理 TcpConnection 的连接和断开
- 错误事件 `OnError`：处理网络 I/O 的错误
- 可读事件 `OnMessage`：有消息到来，需要读取
- 写完成事件 `OnWriteComplete`：之前的写入已经完成

```cpp
class TcpServer : pedrolib::noncopyable, pedrolib::nonmovable {
  std::shared_ptr<EventLoopGroup> boss_group_;
  std::shared_ptr<EventLoopGroup> worker_group_;

  std::shared_ptr<Acceptor> acceptor_;

  ConnectionCallback connection_callback_;
  MessageCallback message_callback_;
  WriteCompleteCallback write_complete_callback_;
  ErrorCallback error_callback_;
  CloseCallback close_callback_;
  HighWatermarkCallback high_watermark_callback_;

  std::mutex mu_;
  std::unordered_set<std::shared_ptr<TcpConnection>> actives_;

public:
  TcpServer() = default;
  ~TcpServer() { Close(); }

  void SetGroup(const std::shared_ptr<EventLoopGroup> &boss,
                const std::shared_ptr<EventLoopGroup> &worker) {
    boss_group_ = boss;
    worker_group_ = worker;
  }

  void Bind(const InetAddress &address);

  void Start();
  void Close();

  void OnConnect(ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
  }

  void OnClose(CloseCallback callback) {
    close_callback_ = std::move(callback);
  }

  void OnMessage(MessageCallback callback) {
    message_callback_ = std::move(callback);
  }

  void OnError(ErrorCallback callback) {
    error_callback_ = std::move(callback);
  }

  void OnWriteComplete(WriteCompleteCallback callback) {
    write_complete_callback_ = std::move(callback);
  }

  void OnHighWatermark(HighWatermarkCallback callback) {
    high_watermark_callback_ = std::move(callback);
  }
};
```

#### 接收一个新连接

`Acceptor` 是 `TcpServer` 接收连接的地方，它运行在 `BossGroup`
的事件循环上。当发生连接事件时，它就会触发 `Acceptor::OnAccept` 回调，告知 `TcpServer` 有新的连接被接收。

```cpp
using AcceptorCallback = std::function<void(Socket)>;

class Acceptor : pedrolib::noncopyable, pedrolib::nonmovable {
public:
  struct Option {
    bool reuse_addr = true;
    bool reuse_port = true;
    bool keep_alive = true;
    bool tcp_no_delay = true;
  };

protected:
  AcceptorCallback acceptor_callback_;
  InetAddress address_;

  SocketChannel channel_;
  EventLoop &eventloop_;

public:
  Acceptor(EventLoop &eventloop, const InetAddress &address,
           const Option &option);

  ~Acceptor() { Close(); }

  void Bind() { channel_.GetFile().Bind(address_); }

  void OnAccept(AcceptorCallback acceptor_callback) {
    acceptor_callback_ = std::move(acceptor_callback);
  }

  void Listen();

  void Close();

  std::string String() const;
};
```

#### 分配连接到 EventLoop 线程

一旦新的连接被接受，它就必须与 `WorkerGroup`
中的某个事件循环绑定，该事件循环将负责该连接的所有I/O读写事件，并触发相应的回调。同时，`TcpServer` 还会将其加入 `actives_`
中，表示所有活跃的 TCP 连接。未来将会加入一个功能：一旦活跃连接数过多，将按照某种策略拒绝连接。

```cpp
PEDRONET_TRACE("TcpServer::OnAccept({})", socket);
auto connection = std::make_shared<TcpConnection>(worker_group_->Next(),
std::move(socket));

connection->OnConnection([this](const TcpConnectionPtr &conn) {
    PEDRONET_TRACE("server raiseConnection: {}", *conn);

    std::unique_lock<std::mutex> lock(mu_);
    actives_.emplace(conn);
    lock.unlock();

    if (connection_callback_) {
        connection_callback_(conn);
    }
});
```

#### 关闭所有连接

由于 `actives_` 记录了所有的活跃连接，我们可以很方便地使用各种策略关闭连接。但无论如何，都需要先关闭 `Acceptor`，防止新的连接加入。

### TcpClient 客户端

`TcpClient` 客户端负责 TCP连接生命周期的管理，连接，读写和关闭。同样地，它也是一个事件驱动思想的组件，因此要使用它，我们需要处理至少三个半事件：

- 连接事件 `OnConnect`/`OnClose`：处理 TcpConnection 的连接和断开
- 错误事件 `OnError`：处理网络 I/O 的错误
- 可读事件 `OnMessage`：有消息到来，需要读取
- 写完成事件 `OnWriteComplete`：之前的写入已经完成

#### TcpClient 的状态

`TcpClient` 有五个状态，与 `TcpConnection` 的状态不一样，TcpClient 多了一个 `kOffline` 的状态，代表 TCP
连接未发起。其余的四个状态和 `TcpConnection` 表示的含义相同。

```cpp
enum class State {
	kOffline,
	kConnecting,
	kConnected,
	kDisconnecting,
	kDisconnected
};
```

#### 发起连接

当调用 `TcpClient::Start()` 时，就会发起 TCP 连接

- 首先，它会将自己的状态设置为 `kConnecting`
- 紧接着，从 `WorkerGroup` 中拿出一个事件循环，并在上面开始建立连接

```cpp
void TcpClient::Start() {
  PEDRONET_TRACE("TcpClient::Start()");

  State s = State::kOffline;
  if (!state_.compare_exchange_strong(s, State::kConnecting)) {
    PEDRONET_WARN("TcpClient::Start() has been invoked");
    return;
  }

  eventloop_ = &worker_group_->Next();
  eventloop_->Run([this] { raiseConnection(); });
}
```

建立连接时，首先它会调用 `socket(2)` 和 `connect(2)` 连接对端服务器：

- 如果能正常连接上：那么调用 `TcpClient::handleConnection` 回调，初始化连接
- 如果出现异常需要重试：那么调用 `TcpClient::retry` 发起重试
- 如果出现未知错误，那么回到 `kOffline` 状态，被告知发生的异常

#### 连接的初始化

当正常连接对端时，便进行连接的初始化：

- `TcpClient` 的状态变更为 `kConnected`
- 构建 `TcpConnection` 对象
- 初始化各种回调

```cpp
void TcpClient::handleConnection(Socket socket) {
  State s = State::kConnecting;
  if (!state_.compare_exchange_strong(s, State::kConnected)) {
    PEDRONET_WARN("state_ != State::kConnection, connection closed");
    return;
  }

  connection_ = std::make_shared<TcpConnection>(*eventloop_, std::move(socket));

  connection_->OnClose([this](auto &&conn) {
    PEDRONET_TRACE("client disconnect: {}", *conn);
    connection_.reset();

    state_ = State::kDisconnected;
    
    if (close_callback_) {
      close_callback_(conn);
    }
  });

  connection_->OnConnection([this](auto &&conn) {
    if (connection_callback_) {
      connection_callback_(conn);
    }
  });

  connection_->OnError(std::move(error_callback_));
  connection_->OnWriteComplete(std::move(write_complete_callback_));
  connection_->OnMessage(std::move(message_callback_));
  connection_->Start();
}

```

#### 连接重试

主动关闭当前的 Socket 文件描述符，并提示异常。然后在本事件循环内调度重试。默认是每秒重试一次（后续会改成配置）

```cpp
void TcpClient::retry(Socket socket, Error reason) {
  socket.Close();
  PEDRONET_TRACE("TcpClient::retry(): {}", reason);
  eventloop_->ScheduleAfter(Duration::Seconds(1), [&] { raiseConnection(); });
}
```

#### 发送请求

`TcpClient::SendPackable` 实际上调用的是 `TcpConnection::SendPackable` 接口。如果客户端的状态不是 `kConnected`
，那么发送请求将会失败。

```cpp
template <class BufferPtr> bool SendPackable(BufferPtr buffer) {
    if (state_ == State::kConnected) {
        connection_->SendPackable(std::move(buffer));
        return true;
    }
    return false;
}
```

#### 关闭连接

用户可以直接调用 `TcpConnection` 关闭连接的接口，也可以调用 `TcpClient` 的相关接口。这些接口都是线程安全的，可以安全地在事件循环外调用。

```cpp
void TcpClient::Close();			// 等时机合适关闭连接
void TcpClient::Shutdown();			// 等待数据写完后关闭写端
void TcpClient::ForceShutdown();	// 强制关闭写端，可能有剩余写数据未处理
void TcpClient::ForceClose();		// 强制关闭连接，可能有剩余读写数据未处理
```

## Pedronet 性能测试

### 实验环境

| CPU     | 13th Gen Inter(R) Core(TM) i9-13900HX 24c32t |
|---------|----------------------------------------------|
| Memory  | Crucial Technology DDR5 5200Mhz 32GiB x2     |
| Kernel  | 5.15.90.1-microsoft-standard-WSL2            |
| GCC     | 11.3.0                                       |
| OS      | Ubuntu 22.04.2 LTS                           |
| Options | SO_{REUSEADDR，KEEPALIVE}，TCP_NODELAY         |

```powershell
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

### 对比对象

- **muduo：** Event-driven network library for multithreaded Linux server in C++11
- **netty：** An asynchronous event-driven network application framework
- **asio：** A cross-platform C++ library for network and low-level I/O programming

### 实验结果

我们采用 PingPong 作为测试方法。每次消息大小分别为 1K，64K，1M。可以发现，消息体大小为 64 K时，吞吐量最大。消息体大小为 1K
时，asio 取得最好的吞吐量。消息体大小为 64K 时，muduo 略占上风。当消息体大小为 1M 时，来自 Java 的 netty 框架吞吐量最高。

|          | 1K (MiB/s) | 1K (Msg/s) | 64K (MiB/s) | 64K (Msg/s) | 1M (MiB/s) | 1M (Msg/s) |
|----------|------------|------------|-------------|-------------|------------|------------|
| pedronet | 3004       | 3034204    | 50102       | 819234      | 27064      | 363064     |
| asio     | 3010       | 3040264    | 48902       | 842876      | 22917      | 359641     |
| netty    | 2830       | 2858454    | 48368       | 776442      | 27816      | 364047     |
| muduo    | 2998       | 3028143    | 50925       | 826535      | 18544      | 297298     |
