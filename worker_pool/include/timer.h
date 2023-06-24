#ifndef PRACTICE_TIMER_H
#define PRACTICE_TIMER_H

#include <chrono>

template<class Procedure> 
inline double duration(const Procedure& proce) {
    using namespace std::chrono_literals;
    auto st = std::chrono::steady_clock::now();
    proce();
    auto et = std::chrono::steady_clock::now();
    return 0.001 * (et - st) / 1us;
}

#endif // PRACTICE_TIMER_H
