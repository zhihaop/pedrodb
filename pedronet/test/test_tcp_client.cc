#include "reporter.h"
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
namespace logger = pedronet::logger;

int main() {
  logger::SetLevel(logger::Level::kInfo);

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

  InetAddress address = InetAddress::Create("127.0.0.1", 1082);
  for (size_t i = 0; i < n_clients; ++i) {
    TcpClient &client = clients.emplace_back(address);
    client.SetGroup(worker_group);
    client.OnConnect([buf](const auto &conn) { conn->Send(buf); });
    client.OnMessage([&reporter](const auto &conn, auto &buffer, auto) {
      reporter.Trace(buffer.ReadableBytes());
      conn->Send(&buffer);
    });
    client.Start();
  }
  reporter.Start(Duration::Seconds(1));
  worker_group->Join();
  return 0;
}