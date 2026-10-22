#include "dce/compute.hpp"
#include "dce/http.hpp"
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <set>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
using namespace dce;
void check(bool ok, const std::string &msg) {
  if (!ok)
    throw std::runtime_error(msg);
}
void pause_ms(int n) {
  std::this_thread::sleep_for(std::chrono::milliseconds(n));
}
struct Process {
  pid_t pid = -1;
  Process(std::vector<std::string> args, const std::string &log) {
    pid = fork();
    if (pid < 0)
      throw std::runtime_error("fork failed");
    if (pid == 0) {
      int fd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0)
        _exit(126);
      dup2(fd, 1);
      dup2(fd, 2);
      close(fd);
      std::vector<char *> av;
      for (auto &s : args)
        av.push_back(s.data());
      av.push_back(nullptr);
      execv(av[0], av.data());
      _exit(127);
    }
  }
  void stop(int sig = SIGTERM) {
    if (pid <= 0)
      return;
    kill(pid, SIGCONT);
    kill(pid, sig);
    for (int i = 0; i < 100; i++) {
      if (waitpid(pid, nullptr, WNOHANG) == pid) {
        pid = -1;
        return;
      }
      pause_ms(10);
    }
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    pid = -1;
  }
  ~Process() {
    stop();
  }
  void await_success() {
    for (int i = 0; i < 1000; ++i) {
      int status = 0;
      if (waitpid(pid, &status, WNOHANG) == pid) {
        pid = -1;
        check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "CLI exited successfully");
        return;
      }
      pause_ms(10);
    }
    throw std::runtime_error("CLI timed out");
  }
};
struct Cluster {
  std::string bin, root, run;
  int port, http_port;
  std::unique_ptr<Process> coordinator;
  std::vector<std::unique_ptr<Process>> workers;
  Cluster(std::string b, std::string r)
      : bin(std::filesystem::absolute(b)), root(std::filesystem::absolute(r)) {
    port = 20000 + getpid() % 15000;
    http_port = port + 1;
    run = root + "/run/verify-" + std::to_string(getpid());
    std::filesystem::create_directories(run);
    start_coordinator();
  }
  json get(const std::string &p) {
    return request("127.0.0.1", http_port, http::verb::get, p);
  }
  template <class F> void until(F f, int timeout = 10000) {
    auto end = Clock::now() + std::chrono::milliseconds(timeout);
    while (Clock::now() < end) {
      if (f())
        return;
      pause_ms(10);
    }
    throw std::runtime_error("condition timed out");
  }
  void start_coordinator() {
    coordinator = std::make_unique<Process>(
        std::vector<std::string>{bin + "/dce-coordinator", "--port", std::to_string(port),
                                 "--http-port", std::to_string(http_port), "--data", root + "/data",
                                 "--dashboard", root + "/web/index.html"},
        run + "/coordinator.log");
    until([&] {
      try {
        return get("/health")["status"] == "ok";
      } catch (...) {
        return false;
      }
    });
  }
  void start_worker(int i) {
    while (workers.size() <= size_t(i))
      workers.push_back(nullptr);
    workers[i] = std::make_unique<Process>(
        std::vector<std::string>{bin + "/dce-worker", "--port", std::to_string(port), "--id",
                                 "worker-" + std::to_string(i), "--threads", "1", "--data",
                                 root + "/data"},
        run + "/worker-" + std::to_string(i) + ".log");
  }
  void count(int n) {
    for (auto &w : workers)
      if (w)
        w->stop();
    workers.clear();
    until([&] {
      int alive = 0;
      for (auto &w : get("/workers"))
        alive += w["alive"].get<bool>();
      return alive == 0;
    });
    for (int i = 0; i < n; i++)
      start_worker(i);
    until([&] {
      int alive = 0;
      for (auto &w : get("/workers"))
        alive += w["alive"].get<bool>();
      return alive == n;
    });
  }
  std::string submit(const json &spec) {
    return request("127.0.0.1", http_port, http::verb::post, "/jobs", spec).at("id");
  }
  json wait(const std::string &id, int timeout = 90000) {
    json j;
    until(
        [&] {
          j = get("/jobs/" + id);
          return j["state"] == "completed" || j["state"] == "failed";
        },
        timeout);
    return j;
  }
};
json prime(uint64_t end, int parts = 12) {
  return {{"type", "primes"}, {"start", 0}, {"end", end}, {"partitions", parts}};
}
void failure_trials(Cluster &c, std::string file, int trials, bool freeze) {
  std::ofstream out(file);
  out << "trial,mode,reassignment_ms,recovered,within_5s\n";
  auto spec = prime(10000000, 6);
  auto expected = serial_reference(spec, c.root + "/data");
  for (int trial = 1; trial <= trials; trial++) {
    c.count(3);
    auto id = c.submit(spec);
    std::string task, attempt, session;
    c.until([&] {
      auto j = c.get("/jobs/" + id);
      for (auto &t : j["tasks"]) {
        for (auto &w : c.get("/workers"))
          if (w["id"] == "worker-0" && w["alive"] == true && t["session"] == w["session"] &&
              t["acknowledged"] == true) {
            task = t["id"];
            attempt = t["attempt_id"];
            session = t["session"];
            return true;
          }
      }
      return false;
    });
    auto start = Clock::now();
    check(kill(c.workers[0]->pid, freeze ? SIGSTOP : SIGKILL) == 0, "signal busy worker");
    double elapsed = -1;
    try {
      c.until(
          [&] {
            auto j = c.get("/jobs/" + id);
            for (auto &t : j["tasks"])
              if (t["id"] == task && t["attempts"].get<int>() > 1 && t["session"] != session &&
                  t["attempt_id"] != attempt && !t["attempt_id"].get<std::string>().empty())
                return true;
            return false;
          },
          10000);
      elapsed = seconds(start, Clock::now()) * 1000;
    } catch (...) {
    }
    if (freeze)
      kill(c.workers[0]->pid, SIGCONT);
    else
      c.workers[0]->stop(SIGKILL);
    auto j = c.wait(id);
    bool ok = j["state"] == "completed" && j["result"] == expected;
    out << trial << ',' << (freeze ? "SIGSTOP" : "SIGKILL") << ',' << elapsed << ',' << ok << ','
        << (elapsed >= 0 && elapsed < 5000) << std::endl;
    check(ok && elapsed >= 0 && elapsed < 5000, "failure recovery trial failed; see " + file);
    if (freeze)
      c.until([&] {
        for (auto &w : c.get("/workers"))
          if (w["id"] == "worker-0" && w["alive"] == true && w["session"] != session)
            return true;
        return false;
      });
    std::cout << "trial " << trial << " " << (freeze ? "SIGSTOP" : "SIGKILL") << " reassigned in "
              << elapsed << " ms; correct result\n";
  }
}
void benchmark(Cluster &c) {
  std::filesystem::create_directories(c.root + "/results");
  std::ofstream out(c.root + "/results/benchmark.csv");
  out << "workload,workers,threads_per_worker,trial,numbers,partitions,wall_seconds,jobs_per_"
         "second,"
         "numbers_per_second,occupied_slot_seconds,available_slot_seconds,utilization\n";
  {
    std::ofstream data(c.root + "/data/benchmark.csv");
    for (int i = 0; i < 500000; ++i)
      data << i % 1000 - 500 << ',' << (i * 17) % 1000 - 500 << '\n';
  }
  for (auto spec :
       {prime(8000000, 24), json{{"type", "csv"}, {"file", "benchmark.csv"}, {"partitions", 24}}}) {
    double numbers = spec["type"] == "primes" ? 8000000 : 1000000;
    auto expected = serial_reference(spec, c.root + "/data");
    for (int n = 1; n <= 3; n++) {
      c.count(n);
      check(c.wait(c.submit(spec))["state"] == "completed", "warmup");
      for (int trial = 1; trial <= 5; trial++) {
        double used0 = 0, avail0 = 0;
        for (auto &w : c.get("/workers"))
          if (w["alive"] == true) {
            used0 += w["occupied_slot_seconds"].get<double>();
            avail0 += w["available_slot_seconds"].get<double>();
          }
        auto start = Clock::now();
        auto j = c.wait(c.submit(spec));
        double elapsed = seconds(start, Clock::now());
        check(j["state"] == "completed", "benchmark completed");
        for (const auto &[key, value] : expected.items())
          check(std::abs(j["result"].at(key).get<double>() - value.get<double>()) <=
                    1e-9 * std::max(1.0, std::abs(value.get<double>())),
                "benchmark correctness");
        double used = 0, avail = 0;
        for (auto &w : c.get("/workers"))
          if (w["alive"] == true) {
            used += w["occupied_slot_seconds"].get<double>();
            avail += w["available_slot_seconds"].get<double>();
          }
        used -= used0;
        avail -= avail0;
        out << spec["type"].get<std::string>() << ',' << n << ",1," << trial << ',' << numbers
            << ",24," << elapsed << ',' << 1 / elapsed << ',' << numbers / elapsed << ',' << used
            << ',' << avail << ',' << used / avail << std::endl;
        std::cout << "benchmark " << spec["type"] << ' ' << n << " workers trial " << trial << ": "
                  << elapsed << " s" << std::endl;
      }
    }
  }
}
int main(int argc, char **argv) {
  try {
    if (argc < 3)
      throw std::runtime_error(
          "dce-verify BUILD_DIR SOURCE_DIR [integration|benchmark|failure|quickstart]");
    std::string mode = argc > 3 ? argv[3] : "integration";
    Cluster c(argv[1], argv[2]);
    if (mode == "benchmark") {
      benchmark(c);
      return 0;
    }
    if (mode == "failure") {
      failure_trials(c, c.root + "/results/failure-kill.csv", 5, false);
      failure_trials(c, c.root + "/results/failure-heartbeat.csv", 3, true);
      return 0;
    }
    c.count(3);
    auto spec = prime(2000000);
    auto id = c.submit(spec);
    auto j = c.wait(id);
    check(j["state"] == "completed" && j["result"] == serial_reference(spec, c.root + "/data"),
          "distributed prime correctness");
    std::set<std::string> sessions;
    for (auto &t : j["tasks"])
      sessions.insert(t["session"]);
    check(sessions.size() == 3, "all three workers execute tasks");
    check(c.get("/jobs/" + id + "/result") == j["result"], "result endpoint");
    // Verify the actual CLI executable against the live API as well as the
    // test client's HTTP requests. Output is machine-readable JSON.
    const auto cli_file = c.run + "/cli.json";
    Process cli({c.bin + "/dce-cli", "demo", "--port", std::to_string(c.http_port), "--end",
                 "100000", "--partitions", "12"},
                cli_file);
    cli.await_success();
    std::ifstream cli_output(cli_file);
    check(json::parse(cli_output)["result"]["count"] == 9592, "CLI demo result");
    {
      std::ofstream f(c.root + "/data/integration.csv");
      for (int i = 0; i < 10000; i++)
        f << i % 1000 - 500 << ',' << (i * 17) % 1000 - 500 << '\n';
    }
    json csv = {{"type", "csv"}, {"file", "integration.csv"}, {"partitions", 17}};
    auto cj = c.wait(c.submit(csv));
    auto ref = serial_reference(csv, c.root + "/data");
    check(cj["state"] == "completed" && cj["result"]["count"] == ref["count"] &&
              std::abs(cj["result"]["variance"].get<double>() - ref["variance"].get<double>()) <
                  1e-7,
          "distributed CSV correctness");
    if (mode != "quickstart") {
      bool rejected = false;
      try {
        c.submit({{"type", "shell"}, {"command", "echo no"}});
      } catch (...) {
        rejected = true;
      }
      check(rejected, "invalid HTTP request rejected");
      {
        std::ofstream f(c.root + "/data/invalid.csv");
        f << "oops\n";
      }
      auto bad = c.wait(c.submit({{"type", "csv"}, {"file", "invalid.csv"}, {"partitions", 1}}));
      check(bad["state"] == "failed" && bad["tasks"][0]["attempts"] == 3,
            "malformed CSV bounded retries");
      std::vector<std::string> jobs;
      auto batch_start = Clock::now();
      for (int i = 0; i < 120; i++)
        jobs.push_back(c.submit(prime(10000, 3)));
      for (auto &x : jobs) {
        auto r = c.wait(x);
        check(r["state"] == "completed" && r["result"]["count"] == 1229, "120 job batch");
      }
      std::ofstream batch(c.run + "/batch.csv");
      batch << "jobs,completed,failed,wall_seconds\n120,120,0,"
            << seconds(batch_start, Clock::now()) << '\n';
      std::cout << "PASS: 120 jobs completed with known correct results\n";
      // A coordinator restart loses memory, but live worker processes must reconnect.
      c.coordinator->stop();
      c.start_coordinator();
      c.until(
          [&] {
            int alive = 0;
            for (auto &w : c.get("/workers"))
              alive += w["alive"].get<bool>();
            return alive == 3;
          },
          15000);
      check(c.wait(c.submit(prime(10000)))["result"]["count"] == 1229,
            "reconnect after coordinator restart");
    }
    failure_trials(c, c.run + "/failure.csv", 1, false);
    if (mode != "quickstart")
      failure_trials(c, c.run + "/heartbeat.csv", 1, true);
    std::cout << "PASS: real three-worker execution, serial comparison, API, failure recovery"
              << (mode != "quickstart" ? ", heartbeat detection and reconnection" : "") << "; logs "
              << c.run << '\n';
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
