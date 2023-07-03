#include "reporter.h"
#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>
#include <pedronet/tcp_client.h>

using namespace std::chrono_literals;
using pedrolib::Duration;
using pedrolib::StaticVector;
using pedronet::BufferView;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;
using pedronet::TcpClient;
namespace logger = pedronet::logger;

void ClientReport(size_t bps, size_t ops, size_t, size_t) {
  double speed = 1.0 * static_cast<double>(bps) / (1 << 20);
  PEDRONET_INFO("client receive: {} MiB/s, {} packages/s", speed, ops);
}

int main() {
  logger::SetLevel(logger::Level::kInfo);

  size_t n_workers = std::thread::hardware_concurrency();
  auto worker_group = EventLoopGroup::Create(n_workers);

  Reporter reporter;
  reporter.SetCallback(ClientReport);
  reporter.Start(*worker_group, Duration::Seconds(1));

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

  worker_group->Join();
  return 0;
}