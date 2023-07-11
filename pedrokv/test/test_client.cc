#include <pedrokv/client.h>
#include <pedrokv/logger/logger.h>
#include <pedronet/logger/logger.h>

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

void TestPut(const std::vector<int> &data) {
  pedrokv::Client client(address, options);
  client.Start();

  std::vector<std::future<pedrokv::Response>> resp;
  resp.reserve(data.size());
  for (int i : data) {
    resp.emplace_back(
        client.Put(fmt::format("hello{}", i),
                   fmt::format("world{}{}", i, Repeat("0", 1 << 10))));
    write_counts++;
  }
  for (auto &r : resp) {
    auto response = r.get();
    if (response.type != pedrokv::Response::Type::kOk) {
      logger.Fatal("error");
    }
  }
  client.Close();
}

void TestGet(const std::vector<int> &data) {
  pedrokv::Client client(address, options);
  client.Start();

  std::vector<std::future<pedrokv::Response>> resp;
  resp.reserve(data.size());
  for (int i : data) {
    resp.emplace_back(client.Get(fmt::format("hello{}", i)));
    read_counts++;
  }
  for (int i = 0; i < data.size(); ++i) {
    auto response = resp[i].get();
    if (response.type != pedrokv::Response::Type::kOk) {
      logger.Fatal("error");
    }
    if (response.data.find(fmt::format("world{}", data[i])) == -1) {
      logger.Fatal("error");
    }
  }
  client.Close();
}

using namespace std::chrono_literals;

int main() {
  // pedrokv::logger::SetLevel(Logger::Level::kInfo);
  // pedronet::logger::SetLevel(Logger::Level::kInfo);

  logger.SetLevel(Logger::Level::kTrace);
  options.worker_group = EventLoopGroup::Create(24);
  options.max_inflight = 128;

  options.worker_group->ScheduleEvery(1s, 1s, [] {
    logger.Info("Puts: {}/s, Gets: {}/s", write_counts.exchange(0),
                read_counts.exchange(0));
  });

  int n = 1000000;
  int m = 1;

  std::vector<std::vector<int>> data(m);
  for (int i = 0; i < n;) {
    for (int j = 0; j < m; ++j, ++i) {
      data[j].emplace_back(i);
    }
  }

  logger.Info("test put start");
  {
    std::vector<std::future<void>> ctx;
    ctx.reserve(m);
    for (int j = 0; j < m; ++j) {
      ctx.emplace_back(
          std::async(std::launch::async, [j, &data] { TestPut(data[j]); }));
    }
  }
  logger.Info("test put end");

  logger.Info("test get start");
  {
    std::vector<std::future<void>> ctx;
    ctx.reserve(m);
    for (int j = 0; j < m; ++j) {
      ctx.emplace_back(
          std::async(std::launch::async, [j, &data] { TestGet(data[j]); }));
    }
  }
  logger.Info("test get end");

  options.worker_group->Close();
  return 0;
}