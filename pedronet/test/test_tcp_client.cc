#include "pedronet/eventloop_impl.h"
#include "pedronet/tcp_client.h"

using namespace std::chrono_literals;
using namespace pedronet;

int main() {
  spdlog::set_level(spdlog::level::info);

  std::atomic_size_t bytes = 0;
  std::atomic_size_t packages = 0;
  auto watcher = std::async(std::launch::async, [&bytes, &packages] {
    while (true) {
      spdlog::info("client bytes receive: {} MB, {} packages",
                   bytes.exchange(0) / 1000000.0, packages.exchange(0));
      std::this_thread::sleep_for(1s);
    }
  });

  TcpClient client(InetAddress::Create("127.0.0.1", 1082));

  size_t n_workers = std::thread::hardware_concurrency();
  auto worker_group = EventLoopGroup::Create<EpollEventLoop>(n_workers);
  worker_group->Start();

  client.SetGroup(worker_group);

  auto buf = std::string(1 << 20, 'a');

  client.OnConnect([buf](auto conn) { conn->Send(buf); });

  client.OnMessage([&bytes, &packages](auto conn, auto buffer, auto now) {
    std::string buf(buffer->ReadableBytes(), 0);
    buffer->Retrieve(buf.data(), buf.size());
    bytes.fetch_add(buf.size());
    packages.fetch_add(1);
    conn->Send(buf);
    //    conn->GetEventLoop().ScheduleAfter(
    //        [conn, buf = std::move(buf)] { conn->Send(buf); },
    //        core::Duration::Seconds(1));
  });

  for (int i = 0; i < 8; ++i) {
    client.Start();
  }
  worker_group->Join();

  return 0;
}