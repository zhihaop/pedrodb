#include <pedronet/eventloopgroup.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_client.h>

using namespace std::chrono_literals;
using pedronet::BufferView;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::TcpClient;
using pedronet::core::Duration;
using pedronet::core::StaticVector;

template <typename T, typename Op> T AtomicApply(std::atomic<T> &val, Op &&op) {
  T n{}, m{};
  do {
    n = val.load();
    m = op(n);
  } while (!val.compare_exchange_strong(n, m));
  return m;
}

class Reporter {
  std::atomic_size_t bytes_{0};
  std::atomic_size_t counts_{0};
  std::atomic_size_t max_bytes_{0};
  std::atomic_size_t min_bytes_{size_t{0} - 1};

  std::function<void(size_t, size_t, size_t, size_t)> callback_;

  uint64_t id_{};
  pedronet::core::Executor &executor_;

public:
  explicit Reporter(pedronet::core::Executor &executor) : executor_(executor) {}
  void Trace(size_t bytes) {
    bytes_.fetch_add(bytes);
    counts_.fetch_add(1);
    AtomicApply(max_bytes_, [bytes](size_t x) { return std::max(x, bytes); });
    AtomicApply(min_bytes_, [bytes](size_t x) { return std::min(x, bytes); });
  }

  void SetCallback(std::function<void(size_t, size_t, size_t, size_t)> cb) {
    callback_ = std::move(cb);
  }

  void Start(const Duration &interval) {
    id_ = executor_.ScheduleEvery(Duration::Zero(), interval, [this] {
      size_t bytes = bytes_.exchange(0);
      size_t count = counts_.exchange(0);
      size_t min_bytes = min_bytes_.exchange(0);
      size_t max_bytes = max_bytes_.exchange(size_t{0} - 1);
      if (callback_) {
        callback_(bytes, count, min_bytes, max_bytes);
      }
    });
  }

  void Close() { executor_.ScheduleCancel(id_); }
};
int main() {
  spdlog::set_level(spdlog::level::info);

  size_t n_workers = std::thread::hardware_concurrency();
  auto worker_group = EventLoopGroup::Create<EpollSelector>(n_workers);
  worker_group->Start();

  Reporter reporter(worker_group->Next());
  reporter.SetCallback([](size_t b, size_t c, size_t, size_t) {
    double speed = 1.0 * static_cast<double>(b) / (1 << 20);
    PEDRONET_INFO("client receive: {} MiB/s, {} packages/s", speed, c);
  });

  auto buf = std::string(1 << 10, 'a');

  size_t n_clients = 128;
  StaticVector<TcpClient> clients(n_clients);

  for (size_t i = 0; i < n_clients; ++i) {
    auto &client = clients.emplace_back(InetAddress::Create("127.0.0.1", 1082));
    client.SetGroup(worker_group);
    client.OnConnect([buf](const auto &conn) { conn->Send(BufferView{buf}); });
    client.OnMessage([&reporter](const auto &conn, auto &buffer, auto) {
      reporter.Trace(buffer.ReadableBytes());
      conn->Send(buffer);
    });
    client.Start();
  }
  reporter.Start(Duration::Seconds(1));

  worker_group->Join();
  return 0;
}