#ifndef PEDRODB_REPORTER_H
#define PEDRODB_REPORTER_H

#include <pedrolib/logger/logger.h>
#include <pedrolib/timestamp.h>

class Reporter {
  using Timestamp = pedrolib::Timestamp;
  using Duration = pedrolib::Duration;
  using Logger = pedrolib::Logger;

  Timestamp start_, last_;
  size_t last_count_{};
  size_t count_{};

  Logger* logger_;
  std::string topic_;

 public:
  explicit Reporter(std::string topic, Logger* log)
      : logger_(log), topic_(std::move(topic)) {
    start_ = last_ = Timestamp::Now();

    logger_->Info("Start reporting {}", topic_);
  }

  ~Reporter() {
    Duration cost = Timestamp::Now() - start_;
    logger_->Info("End report {}: count[{}], cost[{}], avg[{}/ops]", topic_,
                  count_, cost, 1000.0 * (double)count_ / cost.Milliseconds());
  }

  void Report() {
    auto now = Timestamp::Now();
    last_count_++;
    count_++;
    if (now - last_ > Duration::Seconds(1)) {
      logger_->Info("Report {}: {}ops/s", topic_, last_count_);
      last_ = now;
      last_count_ = 0;
    }
  }
};
#endif  //PEDRODB_REPORTER_H
