#include <pedronet/eventloopgroup.h>
#include <chrono>
#include <iostream>
using namespace std::chrono_literals;
using pedronet::EventLoopGroup;

int main() {
    auto g = EventLoopGroup::Create(1);
    g->ScheduleEvery(0s, 1s, [=] {
        std::cout << "hello world" << std::endl;
    });
    return 0;
}