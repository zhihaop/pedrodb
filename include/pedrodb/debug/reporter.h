#ifndef PEDRODB_DEBUG_REPORTER_H
#define PEDRODB_DEBUG_REPORTER_H

#include <pedrolib/executor/thread_pool_executor.h>
#include <pedrolib/logger/logger.h>
#include <pedrolib/timestamp.h>

namespace pedrodb::debug {
class Reporter {
  using Timestamp = pedrolib::Timestamp;
  using Duration = pedrolib::Duration;
  using Logger = pedrolib::Logger;
  using Executor = pedrolib::Executor;

  Timestamp start_;
  std::atomic_size_t last_count_{};
  std::atomic_size_t count_{};

  Logger* logger_;
  std::string topic_;

  uint64_t timer_;

  static Executor& GetDefaultExecutor() {
    static pedrolib::ThreadPoolExecutor executor(1);
    return executor;
  }

 public:
  explicit Reporter(std::string topic, Logger* log)
      : logger_(log), topic_(std::move(topic)) {
    start_ = Timestamp::Now();
    logger_->Info("Start reporting {}", topic_);

    using namespace std::chrono_literals;
    timer_ = GetDefaultExecutor().ScheduleEvery(1s, 1s, [this] {
      logger_->Info("Report {}: {}ops/s", topic_, last_count_.exchange(0));
    });
  }

  ~Reporter() {
    Duration cost = Timestamp::Now() - start_;
    logger_->Info("End report {}: count[{}], cost[{}], avg[{}/ops]", topic_,
                  count_, cost,
                  1000.0 * (double)count_ / (double)cost.Milliseconds());

    GetDefaultExecutor().ScheduleCancel(timer_);
  }

  void Report() {
    last_count_++;
    count_++;
  }
};
}  // namespace pedrodb::debug
#endif  //PEDRODB_DEBUG_REPORTER_H
