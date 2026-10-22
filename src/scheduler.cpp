#include "dce/scheduler.hpp"
namespace dce {
void Scheduler::event(std::string type, json d, Clock::time_point now) {
  d["type"] = type;
  d["elapsed_ms"] = seconds(started_, now) * 1000;
  events_.push_back(d);
  if (events_.size() > 10000)
    events_.pop_front();
  log(type, d);
}
void Scheduler::account(Worker &w, Clock::time_point now) {
  if (w.alive) {
    auto dt = seconds(w.accounted, now);
    w.occupied += dt * w.tasks.size();
    w.available += dt * w.capacity;
    w.accounted = now;
  }
}
std::string Scheduler::submit(const json &spec, Clock::time_point now) {
  auto parts = partition(spec, root_);
  std::lock_guard l(mutex_);
  if (jobs_.size() >= 10000)
    throw std::runtime_error("job retention limit reached; restart coordinator");
  Job j;
  j.id = uid();
  j.type = spec.at("type");
  j.spec = spec;
  j.created = now;
  for (auto &p : parts) {
    Task t;
    t.id = uid();
    t.job = j.id;
    t.payload = p;
    j.tasks.push_back(t.id);
    pending_.push_back(t.id);
    tasks_.emplace(t.id, std::move(t));
  }
  auto id = j.id;
  jobs_.emplace(id, std::move(j));
  event("submitted", {{"job", id}}, now);
  return id;
}
std::string Scheduler::add_worker(std::string id, int capacity, Clock::time_point now) {
  if (id.empty() || id.size() > 128 || capacity < 1 || capacity > 64)
    throw std::runtime_error("invalid worker registration");
  std::lock_guard l(mutex_);
  if (workers_.size() >= 10000)
    throw std::runtime_error("session retention limit reached");
  for (auto &[k, w] : workers_)
    if (w.id == id && w.alive)
      lose(w, "replaced_session", now);
  Worker w;
  w.id = id;
  w.session = uid();
  w.capacity = capacity;
  w.heartbeat = w.joined = w.accounted = now;
  auto s = w.session;
  workers_.emplace(s, std::move(w));
  event("registered", {{"worker", id}, {"session", s}}, now);
  return s;
}
void Scheduler::heartbeat(const std::string &s, Clock::time_point now) {
  std::lock_guard l(mutex_);
  auto i = workers_.find(s);
  if (i == workers_.end() || !i->second.alive)
    throw std::runtime_error("expired session");
  i->second.heartbeat = now;
}
void Scheduler::requeue(Task &t, std::string reason, Clock::time_point now) {
  auto &j = jobs_.at(t.job);
  t.error = std::move(reason);
  t.worker.clear();
  t.attempt.clear();
  t.ack = false;
  if (j.state == "failed" || t.tries >= retries_) {
    t.state = "failed";
    j.state = "failed";
    j.finished = now;
    for (auto &id : j.tasks)
      if (tasks_.at(id).state == "queued")
        tasks_.at(id).state = "failed";
    event("task_failed", {{"task", t.id}, {"job", j.id}, {"error", t.error}}, now);
  } else {
    t.state = "queued";
    pending_.push_back(t.id);
    event("requeued", {{"task", t.id}, {"reason", t.error}}, now);
  }
}
void Scheduler::lose(Worker &w, std::string reason, Clock::time_point now) {
  if (!w.alive)
    return;
  account(w, now);
  w.alive = false;
  auto ids = w.tasks;
  w.tasks.clear();
  event("worker_lost", {{"worker", w.id}, {"session", w.session}, {"reason", reason}}, now);
  for (auto &id : ids)
    requeue(tasks_.at(id), reason, now);
}
void Scheduler::disconnect(const std::string &s, Clock::time_point now) {
  std::lock_guard l(mutex_);
  auto i = workers_.find(s);
  if (i != workers_.end())
    lose(i->second, "disconnect", now);
}
void Scheduler::acknowledge(const std::string &s, const json &m) {
  std::lock_guard l(mutex_);
  auto i = tasks_.find(m.at("task_id").get<std::string>());
  if (i != tasks_.end() && i->second.state == "running" && i->second.worker == s &&
      i->second.attempt == m.at("attempt_id").get<std::string>())
    i->second.ack = true;
}
bool Scheduler::finish(const std::string &s, const json &m, Clock::time_point now) {
  std::lock_guard l(mutex_);
  auto i = tasks_.find(m.at("task_id").get<std::string>());
  if (i == tasks_.end())
    return false;
  auto &t = i->second;
  auto w = workers_.find(s);
  if (t.state != "running" || t.worker != s || t.attempt != m.at("attempt_id").get<std::string>() ||
      w == workers_.end() || !w->second.alive) {
    event("stale_result", {{"task", t.id}}, now);
    return false;
  }
  bool success = m.at("type") == "complete";
  // Validate the whole message before changing slot ownership. A malformed
  // message must leave the task recoverable when its connection is closed.
  std::string error = success ? "" : m.value("error", std::string("worker failure")).substr(0, 512);
  json r;
  if (success) {
    r = m.at("result");
    if (!r.is_object() || !r.contains("count") || !r["count"].is_number_unsigned())
      throw std::runtime_error("invalid result");
    if (jobs_.at(t.job).type == "primes" &&
        r["count"].get<uint64_t>() >
            t.payload["end"].get<uint64_t>() - t.payload["begin"].get<uint64_t>())
      throw std::runtime_error("prime count exceeds task range");
    if (jobs_.at(t.job).type == "csv") {
      if (r["count"].get<uint64_t>() > 64 * 1024 * 1024)
        throw std::runtime_error("CSV count exceeds input limit");
      for (const auto *field : {"sum", "m2", "mean", "min", "max"}) {
        if (r["count"] == 0 && (std::string(field) == "mean" || std::string(field) == "min" ||
                                std::string(field) == "max"))
          continue;
        if (!r.at(field).is_number() || !std::isfinite(r.at(field).get<double>()))
          throw std::runtime_error("invalid CSV result statistic");
      }
      if (r.at("m2").get<double>() < 0)
        throw std::runtime_error("negative squared deviation");
      aggregate("csv", {r});
    }
  }
  account(w->second, now);
  w->second.tasks.erase(t.id);
  if (!success) {
    requeue(t, error, now);
    return true;
  }
  auto &j = jobs_.at(t.job);
  if (j.state == "failed") {
    t.state = "failed";
    return false;
  }
  t.result = r;
  t.state = "completed";
  event("completed_task", {{"task", t.id}, {"attempt", t.attempt}, {"worker", w->second.id}}, now);
  bool done = true;
  std::vector<json> results;
  for (auto &id : j.tasks) {
    auto &x = tasks_.at(id);
    done &= x.state == "completed";
    results.push_back(x.result);
  }
  if (done) {
    j.result = aggregate(j.type, results);
    j.state = "completed";
    j.finished = now;
    event("completed_job", {{"job", j.id}}, now);
  }
  return true;
}
std::vector<std::string> Scheduler::expire(Clock::time_point now) {
  std::lock_guard l(mutex_);
  std::vector<std::string> dead;
  for (auto &[s, w] : workers_) {
    if (!w.alive)
      continue;
    std::string reason;
    if (now - w.heartbeat > timeout_)
      reason = "heartbeat_timeout";
    for (auto &id : w.tasks) {
      auto &t = tasks_.at(id);
      if (now - t.assigned > lease_)
        reason = "execution_lease_expired";
      else if (!t.ack && now - t.assigned > std::chrono::seconds(1))
        reason = "ack_timeout";
    }
    if (!reason.empty()) {
      dead.push_back(s);
      lose(w, reason, now);
    }
  }
  return dead;
}
std::vector<Assignment> Scheduler::dispatch(Clock::time_point now) {
  std::lock_guard l(mutex_);
  std::vector<Assignment> out;
  while (!pending_.empty()) {
    auto &t = tasks_.at(pending_.front());
    if (t.state != "queued" || jobs_.at(t.job).state == "failed") {
      pending_.pop_front();
      continue;
    }
    Worker *best = nullptr;
    for (auto &[s, w] : workers_)
      if (w.alive && w.tasks.size() < size_t(w.capacity)) {
        if (!best ||
            double(w.tasks.size()) / w.capacity < double(best->tasks.size()) / best->capacity ||
            (double(w.tasks.size()) / w.capacity == double(best->tasks.size()) / best->capacity &&
             w.assigned < best->assigned))
          best = &w;
      }
    if (!best)
      break;
    pending_.pop_front();
    account(*best, now);
    t.worker = best->session;
    t.attempt = uid();
    t.assigned = now;
    t.tries++;
    t.ack = false;
    t.state = "running";
    best->tasks.insert(t.id);
    best->assigned++;
    jobs_.at(t.job).state = "running";
    out.push_back({best->session,
                   {{"type", "assign"},
                    {"job_id", t.job},
                    {"task_id", t.id},
                    {"attempt_id", t.attempt},
                    {"payload", t.payload}}});
    event("assigned",
          {{"task", t.id},
           {"job", t.job},
           {"worker", best->id},
           {"session", best->session},
           {"attempt", t.attempt},
           {"try", t.tries}},
          now);
  }
  return out;
}
json Scheduler::job_view(const Job &j, Clock::time_point now) const {
  json ts = json::array();
  int completed = 0;
  for (auto &id : j.tasks) {
    auto &t = tasks_.at(id);
    completed += t.state == "completed";
    ts.push_back({{"id", id},
                  {"state", t.state},
                  {"session", t.worker},
                  {"attempt_id", t.attempt},
                  {"attempts", t.tries},
                  {"acknowledged", t.ack},
                  {"error", t.error}});
  }
  return {
      {"id", j.id},
      {"type", j.type},
      {"state", j.state},
      {"spec", j.spec},
      {"tasks", ts},
      {"completed_tasks", completed},
      {"total_tasks", j.tasks.size()},
      {"elapsed_seconds", seconds(j.created, j.finished == Clock::time_point{} ? now : j.finished)},
      {"result", j.result}};
}
json Scheduler::job(const std::string &id) const {
  std::lock_guard l(mutex_);
  return job_view(jobs_.at(id), Clock::now());
}
json Scheduler::jobs() const {
  std::lock_guard l(mutex_);
  json a = json::array();
  for (auto &[id, j] : jobs_)
    a.push_back(job_view(j, Clock::now()));
  return a;
}
json Scheduler::workers(Clock::time_point now) {
  std::lock_guard l(mutex_);
  json a = json::array();
  for (auto &[s, w] : workers_) {
    account(w, now);
    a.push_back({{"id", w.id},
                 {"session", s},
                 {"alive", w.alive},
                 {"capacity", w.capacity},
                 {"load", w.tasks.size()},
                 {"heartbeat_age_ms", seconds(w.heartbeat, now) * 1000},
                 {"assigned_tasks", w.assigned},
                 {"occupied_slot_seconds", w.occupied},
                 {"available_slot_seconds", w.available},
                 {"idle_slot_seconds", w.available - w.occupied},
                 {"utilization", w.available ? w.occupied / w.available : 0}});
  }
  return a;
}
json Scheduler::events() const {
  std::lock_guard l(mutex_);
  return events_;
}
} // namespace dce
