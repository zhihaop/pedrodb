#include <pedronet/eventloopgroup.h>
#include <pedronet/logger/logger.h>
#include <pedronet/selector/epoller.h>

#include <iostream>

using namespace std::chrono_literals;
using pedrolib::Duration;
using pedronet::EpollSelector;
using pedronet::EventLoopGroup;
namespace logger = pedronet::logger;

int main() {
  logger::SetLevel(logger::Level::kInfo);

  auto group = EventLoopGroup::Create<EpollSelector>(12);

  group->Schedule([] { std::cout << "hello world" << std::endl; });

  for (int i = 0; i < 5; ++i) {
    group->Schedule([=] { std::cout << "hello world" << i << std::endl; });
  }

  int i = 0;
  auto id = group->ScheduleEvery(500ms, 500ms, [&] {
    std::cout << "hello world timer 1:" << ++i << std::endl;
  });

  int j = 0;
  group->ScheduleEvery(0ms, 1000ms, [&] {
    std::cout << "hello world timer 2:" << ++j << std::endl;
    if (j == 15) {
      group->ScheduleCancel(id);
    }
    if (j == 20) {
      group->Close();
    }
  });
  return 0;
}