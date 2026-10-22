#pragma once
#include "compute.hpp"
#include <deque>
#include <mutex>
#include <set>
namespace dce {
struct Assignment {
  std::string session;
  json message;
};
class Scheduler {
  struct Task {
    std::string id, job, state = "queued", worker, attempt, error;
    json payload, result;
    int tries = 0;
    bool ack = false;
    Clock::time_point assigned{};
  };
  struct Job {
    std::string id, type, state = "queued";
    std::vector<std::string> tasks;
    json spec, result;
    Clock::time_point created, finished{};
  };
  struct Worker {
    std::string id, session;
    int capacity = 0;
    bool alive = true;
    std::set<std::string> tasks;
    Clock::time_point heartbeat, joined, accounted;
    double occupied = 0, available = 0;
    uint64_t assigned = 0;
  };
  mutable std::mutex mutex_;
  std::map<std::string, Task> tasks_;
  std::map<std::string, Job> jobs_;
  std::map<std::string, Worker> workers_;
  std::deque<std::string> pending_;
  std::deque<json> events_;
  std::filesystem::path root_;
  int retries_;
  std::chrono::milliseconds timeout_, lease_;
  Clock::time_point started_ = Clock::now();
  void event(std::string type, json data, Clock::time_point now);
  void account(Worker &, Clock::time_point);
  void requeue(Task &, std::string, Clock::time_point);
  void lose(Worker &, std::string, Clock::time_point);
  json job_view(const Job &, Clock::time_point) const;

public:
  Scheduler(std::filesystem::path root, int attempts = 3, int timeout_ms = 2000,
            int lease_ms = 60000)
      : root_(std::move(root)), retries_(attempts), timeout_(timeout_ms), lease_(lease_ms) {}
  std::string submit(const json &spec, Clock::time_point now = Clock::now());
  std::string add_worker(std::string id, int capacity, Clock::time_point now = Clock::now());
  void heartbeat(const std::string &, Clock::time_point now = Clock::now());
  void disconnect(const std::string &, Clock::time_point now = Clock::now());
  void acknowledge(const std::string &, const json &);
  bool finish(const std::string &, const json &, Clock::time_point now = Clock::now());
  std::vector<std::string> expire(Clock::time_point now = Clock::now());
  std::vector<Assignment> dispatch(Clock::time_point now = Clock::now());
  json jobs() const;
  json job(const std::string &) const;
  json workers(Clock::time_point now = Clock::now());
  json events() const;
};
} // namespace dce
