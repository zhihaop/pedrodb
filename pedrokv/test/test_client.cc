#include <pedrokv/client.h>
#include <pedrokv/logger/logger.h>
#include <pedronet/logger/logger.h>

#include <random>

using pedrokv::ClientOptions;
using pedrolib::Logger;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;

static auto logger = Logger("test");
auto address = InetAddress::Create("127.0.0.1", 1082);
auto options = ClientOptions{};
std::atomic_size_t write_counts;
std::atomic_size_t read_counts;

std::string Repeat(std::string_view s, size_t n) {
  std::string t;
  t.reserve(s.size() * n);
  while (n--) {
    t += s;
  }
  return t;
}

void TestSyncPut(pedrokv::Client& client, const std::vector<int>& data) {
  for (int i : data) {
    auto response =
        client.Put(fmt::format("hello{}", i),
                   fmt::format("world{}{}", i, Repeat("0", 1 << 10)));

    if (response.type != pedrokv::Response::Type::kOk) {
      logger.Fatal("error");
    }
    write_counts++;
  }
}

void TestAsyncPut(pedrokv::Client& client, const std::vector<int>& data) {
  pedrolib::Latch latch(data.size());
  for (int i : data) {
    client.Put(fmt::format("hello{}", i),
               fmt::format("world{}{}", i, Repeat("0", 1 << 10)),
               [&latch](const auto& resp) {
                 if (resp.type != pedrokv::Response::Type::kOk) {
                   logger.Fatal("error");
                 }
                 write_counts++;
                 latch.CountDown();
               });
  }
  latch.Await();
}

void TestSyncGet(pedrokv::Client& client, std::vector<int> data) {
  std::shuffle(data.begin(), data.end(), std::mt19937(std::random_device()()));
  for (int i : data) {
    auto response = client.Get(fmt::format("hello{}", i));
    if (response.type != pedrokv::Response::Type::kOk) {
      logger.Fatal("error type");
    }
    if (response.data.find(fmt::format("world{}", i)) == -1) {
      logger.Fatal("error value");
    }
    read_counts++;
  }
}

void TestAsyncGet(pedrokv::Client& client, std::vector<int> data) {
  std::shuffle(data.begin(), data.end(), std::mt19937(std::random_device()()));
  pedrolib::Latch latch(data.size());
  for (int i : data) {
    client.Get(fmt::format("hello{}", i), [i, &latch](auto&& response) {
      if (response.type != pedrokv::Response::Type::kOk) {
        logger.Fatal("error type");
      }
      if (response.data.find(fmt::format("world{}", i)) == -1) {
        logger.Fatal("error value");
      }
      read_counts++;
      latch.CountDown();
    });
  }
  latch.Await();
}

using namespace std::chrono_literals;

void TestAsync(int n) {
  pedrokv::Client client(address, options);
  client.Start();

  std::vector<int> data(n);
  std::iota(data.begin(), data.end(), 0);

  logger.Info("test async put start");
  { TestAsyncPut(client, data); }
  logger.Info("test async end");

  logger.Info("test async get start");
  { TestAsyncGet(client, data); }
  logger.Info("test async get end");

  client.Close();
}

void TestSync(int n, int m, int c) {
  std::vector<std::shared_ptr<pedrokv::Client>> clients(c);
  for (int i = 0; i < c; ++i) {
    clients[i] = std::make_shared<pedrokv::Client>(address, options);
    clients[i]->Start();
  }
  std::vector<std::vector<int>> data(m);
  for (int i = 0; i < n;) {
    for (int j = 0; j < m; ++j, ++i) {
      data[j].emplace_back(i);
    }
  }

  logger.Info("test sync put start");
  {
    std::vector<std::future<void>> ctx;
    ctx.reserve(m);
    for (int j = 0; j < m; ++j) {
      ctx.emplace_back(std::async(std::launch::async, [&, j] {
        TestSyncPut(*clients[j % clients.size()], data[j]);
      }));
    }
  }
  logger.Info("test sync end");

  logger.Info("test sync get start");
  {
    std::vector<std::future<void>> ctx;
    ctx.reserve(m);
    for (int j = 0; j < m; ++j) {
      ctx.emplace_back(std::async(std::launch::async, [&, j] {
        TestSyncGet(*clients[j % clients.size()], data[j]);
      }));
    }
  }
  logger.Info("test sync get end");

  for (auto& client : clients) {
    client->Close();
    client.reset();
  }
}

int main() {
  pedrokv::logger::SetLevel(Logger::Level::kError);
  // pedronet::logger::SetLevel(Logger::Level::kInfo);

  logger.SetLevel(Logger::Level::kTrace);
  options.worker_group = EventLoopGroup::Create();
  options.max_inflight = 1024;

  options.worker_group->ScheduleEvery(1s, 1s, [] {
    logger.Info("Puts: {}/s, Gets: {}/s", write_counts.exchange(0),
                read_counts.exchange(0));
  });

  int n = 2000000;
  TestAsync(n);
  TestSync(n, 50, 50);

  options.worker_group->Close();
  return 0;
}