#include <pedronet/eventloopgroup.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_client.h>

using namespace std::chrono_literals;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::TcpClient;
using pedronet::core::Duration;

int main() {
  spdlog::set_level(spdlog::level::info);

  size_t n_workers = std::thread::hardware_concurrency();
  auto worker_group = EventLoopGroup::Create<EpollSelector>(n_workers);
  worker_group->Start();
  std::atomic_size_t bytes = 0;
  std::atomic_size_t packages = 0;

  worker_group->ScheduleEvery(0s, 1s, [&bytes, &packages]() {
    double speed = 1.0 * bytes.exchange(0) / (1 << 20);
    size_t tps = packages.exchange(0);
    spdlog::info("client receive: {} MiB/s, {} packages/s", speed, tps);
  });

  TcpClient client(InetAddress::Create("127.0.0.1", 1082));

  client.SetGroup(worker_group);

  auto buf = std::string(1 << 10, 'a');

  client.OnConnect([buf](auto conn) { conn->Send(buf); });

  client.OnMessage([&bytes, &packages](auto conn, auto buffer, auto now) {
    bytes.fetch_add(buffer->ReadableBytes());
    packages.fetch_add(1);
    conn->Send(buffer);
  });

  for (int i = 0; i < 128; ++i) {
    client.Start();
  }
  worker_group->Join();

  return 0;
}