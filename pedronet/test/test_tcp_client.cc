#include "pedronet/eventloop_impl.h"
#include "pedronet/tcp_client.h"

using namespace std::chrono_literals;
using namespace pedronet;

int main() {
  spdlog::set_level(spdlog::level::info);

  std::atomic_size_t bytes = 0;
  auto watcher = std::async(std::launch::async, [&bytes] {
    while (true) {
      spdlog::info("client bytes receive: {} MB",
                   bytes.exchange(0) / 1000000.0);
      std::this_thread::sleep_for(1s);
    }
  });

  TcpClient client(InetAddress::Create("127.0.0.1", 1082));

  size_t n_workers = std::thread::hardware_concurrency();
  auto worker_group = EventLoopGroup::Create<EpollEventLoop>(n_workers);
  worker_group->Start();

  client.SetGroup(worker_group);

  client.OnConnect([](auto conn) { conn->Send("hello server"); });

  client.OnMessage([&bytes](auto conn, auto buffer, auto now) {
    std::string buf(buffer->ReadableBytes(), 0);
    buffer->Retrieve(buf.data(), buf.size());
    bytes.fetch_add(buf.size());
    conn->GetEventLoop().ScheduleAfter(
        [conn, buf = std::move(buf)] { conn->Send(buf); },
        core::Duration::Seconds(1));
  });

  for (int i = 0; i < 256; ++i) {
    client.Start();
  }
  worker_group->Join();

  return 0;
}