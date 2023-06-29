#include <pedronet/channel/channel.h>
#include <pedronet/eventloop_impl.h>

#include <iostream>
#include <spdlog/common.h>
#include "pedronet/core/debug.h"
using namespace std;


int main() {
  spdlog::set_level(spdlog::level::trace);
  auto group = pedronet::EventLoopGroup::Create<pedronet::EpollEventLoop>(12);
  group->Start();

  std::string s(10, 0);
  cout << s.size() << endl;

  auto &loop = group->Next();
  loop.Submit([] { std::cout << "hello world" << std::endl; });

  for (int i = 0; i < 5; ++i) {
    loop.Submit([=] { std::cout << "hello world" << i << std::endl; });
  }

  int i = 0;
  auto id = loop.ScheduleEvery(
      [&i] { std::cout << "hello world timer 1:" << ++i << std::endl; },
      pedronet::core::Duration::Milliseconds(500),
      pedronet::core::Duration::Milliseconds(500));

  int j = 0;
  loop.ScheduleEvery(
      [&j, &loop, id, &group] {
        std::cout << "hello world timer 2:" << ++j << std::endl;
        if (j == 15) {
          loop.ScheduleCancel(id);
          group->Close();
        }
      },
      pedronet::core::Duration::Seconds(0),
      pedronet::core::Duration::Milliseconds(1000));
  cout << "hello test channel" << endl;
  return 0;
}