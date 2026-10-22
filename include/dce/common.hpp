#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <json.hpp>
#include <map>
#include <random>
#include <string>
namespace dce {
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
inline double seconds(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}
inline std::string uid() {
  static std::atomic<uint64_t> n{0};
  // A random 128-bit process prefix plus a monotonic counter. IDs remain
  // distinct across task attempts, sessions, jobs, and coordinator restarts.
  static const auto seed = [] {
    std::random_device random;
    return std::to_string(random()) + "-" + std::to_string(random()) + "-" +
           std::to_string(random()) + "-" + std::to_string(random());
  }();
  return seed + "-" + std::to_string(++n);
}
inline void log(const std::string &event, json fields = json::object()) {
  fields["event"] = event;
  fields["unix_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
  std::cerr << fields.dump() << '\n';
}
struct Args {
  std::map<std::string, std::string> opts;
  Args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
      std::string k = argv[i];
      if (!k.starts_with("--") || i + 1 == argc)
        throw std::runtime_error("expected --option value");
      opts[k.substr(2)] = argv[++i];
    }
  }
  std::string get(std::string k, std::string d) const {
    auto i = opts.find(k);
    return i == opts.end() ? d : i->second;
  }
  int number(std::string k, int d, int lo, int hi) const {
    auto s = get(k, std::to_string(d));
    size_t p;
    int n = std::stoi(s, &p);
    if (p != s.size() || n < lo || n > hi)
      throw std::runtime_error("invalid " + k);
    return n;
  }
};
} // namespace dce
