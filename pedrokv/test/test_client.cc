#include <pedrodb/debug/data.h>
#include <pedrodb/debug/reporter.h>
#include <pedrokv/client.h>
#include <pedrokv/logger/logger.h>

#include <random>

using pedrodb::debug::Generator;
using pedrodb::debug::KeyValue;
using pedrodb::debug::KeyValueOptions;
using pedrodb::debug::Reporter;
using pedrokv::Client;
using pedrokv::ClientOptions;
using pedrokv::ResponseType;
using pedrokv::SyncClient;
using pedrolib::Latch;
using pedrolib::Logger;
using pedrolib::ThreadPoolExecutor;
using pedronet::EventLoopGroup;
using pedronet::InetAddress;

static auto logger = Logger("test");

struct TestOptions {
  ClientOptions client;
  InetAddress address = InetAddress::Create("127.0.0.1", 1082);

  bool enable_read = true;
  bool enable_write = false;
  KeyValueOptions data{};
} options;

void TestAsync(int n, int p, int c);
void TestSync(int n, int p, int c);

int main() {
  pedrokv::logger::SetLevel(Logger::Level::kInfo);

  logger.SetLevel(Logger::Level::kTrace);
  options.client.worker_group = EventLoopGroup::Create();
  options.client.max_inflight = 1024;
  options.enable_write = true;
  options.data.key_size = 16;
  options.data.value_size = 100;
  options.data.lazy_value = true;
  options.data.random_value = true;

  int n = 2000000;
  for (int i = 1; i <= 64; ++i) {
    TestAsync(n, i, std::min(i, 8));
    TestSync(n, i, i);
  }
  return 0;
}

void TestSyncPut(Reporter& reporter, SyncClient& client,
                 const std::vector<KeyValue>& data) {
  for (auto& kv : data) {
    auto response = client.Put(kv.key(), kv.value());
    if (response.type != ResponseType::kOk) {
      logger.Fatal("error: {}", response.data);
    }
    reporter.Report();
  }
}

void TestAsyncPut(Reporter& reporter, Client& client,
                  const std::vector<KeyValue>& data) {
  Latch latch(data.size());
  for (auto& kv : data) {
    client.Put(kv.key(), kv.value(), [&](const auto& resp) {
      if (resp.type != ResponseType::kOk) {
        logger.Fatal("error: {}", resp.data);
      }
      reporter.Report();
      latch.CountDown();
    });
  }
  latch.Await();
}

void TestSyncGet(Reporter& reporter, SyncClient& client,
                 std::vector<KeyValue> data) {
  std::shuffle(data.begin(), data.end(), std::mt19937(std::random_device()()));

  for (auto& kv : data) {
    auto response = client.Get(kv.key());
    if (response.type != ResponseType::kOk) {
      logger.Fatal("error: {}", response.data);
    }
    if (kv.value() != response.data) {
      logger.Fatal("error value");
    }
    reporter.Report();
  }
}

void TestAsyncGet(Reporter& reporter, Client& client,
                  std::vector<KeyValue> data) {
  std::shuffle(data.begin(), data.end(), std::mt19937(std::random_device()()));
  Latch latch(data.size());
  for (auto& kv : data) {
    client.Get(kv.key(), [&, value = kv.value()](auto&& response) {
      if (response.type != pedrokv::ResponseType::kOk) {
        logger.Fatal("error: {}", response.data);
      }
      if (response.data != value) {
        logger.Fatal("error value");
      }
      reporter.Report();
      latch.CountDown();
    });
  }
  latch.Await();
}

template <class Client>
struct Group {
  Client client;
  std::vector<KeyValue> data;
};

template <class Client>
std::vector<Group<Client>> DispatchGroup(const std::vector<Client>& clients,
                                         const std::vector<KeyValue>& data,
                                         size_t g) {
  size_t n = data.size();
  size_t m = n / g;
  size_t t = n % g;

  std::vector<Group<Client>> groups(g);
  auto it = data.begin();
  for (int i = 0; i < g; ++i) {
    groups[i].client = clients[i % clients.size()];
    groups[i].data.resize(m + (i < t));
    for (auto& x : groups[i].data) {
      x = *(it++);
    }
  }

  return groups;
}

void TestSync(int n, int p, int c) {
  using Client = pedrokv::SyncClient;

  std::vector<Client::Ptr> clients(c);
  for (auto& client : clients) {
    client = std::make_shared<Client>(options.address, options.client);
    client->Start();
  }

  auto groups = DispatchGroup(clients, Generator(n, options.data), p);

  ThreadPoolExecutor executor(p);
  if (options.enable_write) {
    Reporter reporter(fmt::format("SyncPut-{}", p), &logger);
    pedrolib::for_each(&executor, groups.begin(), groups.end(), [&](auto& g) {
      TestSyncPut(reporter, *g.client, g.data);
    });
  }

  if (options.enable_read) {
    Reporter reporter(fmt::format("SyncGet-{}", p), &logger);
    pedrolib::for_each(&executor, groups.begin(), groups.end(), [&](auto& g) {
      TestSyncGet(reporter, *g.client, g.data);
    });
  }
}

void TestAsync(int n, int p, int c) {
  using Client = pedrokv::Client;

  std::vector<Client::Ptr> clients(c);
  for (auto& client : clients) {
    client = std::make_shared<Client>(options.address, options.client);
    client->Start();
  }

  auto groups = DispatchGroup(clients, Generator(n, options.data), p);

  ThreadPoolExecutor executor(p);
  if (options.enable_write) {
    Reporter reporter(fmt::format("AsyncPut-{}", p), &logger);
    pedrolib::for_each(&executor, groups.begin(), groups.end(), [&](auto& g) {
      TestAsyncPut(reporter, *g.client, g.data);
    });
  }

  if (options.enable_read) {
    Reporter reporter(fmt::format("AsyncGet-{}", p), &logger);
    pedrolib::for_each(&executor, groups.begin(), groups.end(), [&](auto& g) {
      TestAsyncGet(reporter, *g.client, g.data);
    });
  }
}
