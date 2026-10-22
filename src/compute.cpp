#include "dce/compute.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
namespace dce {
namespace {
uint64_t integer(const json &j, const char *key, uint64_t lo, uint64_t hi) {
  if (!j.contains(key) || !j[key].is_number_integer() || j[key].get<double>() < double(lo) ||
      j[key].get<double>() > double(hi))
    throw std::runtime_error(std::string("invalid ") + key);
  return j[key].get<uint64_t>();
}
struct Stats {
  uint64_t n = 0;
  long double sum = 0, mean = 0, m2 = 0;
  double min = 0, max = 0;
  void add(double x) {
    if (!std::isfinite(x) || std::abs(x) > 1e100)
      throw std::runtime_error("CSV value must be finite and <= 1e100 in magnitude");
    if (!n)
      min = max = x;
    else {
      min = std::min(min, x);
      max = std::max(max, x);
    }
    ++n;
    sum += x;
    auto d = x - mean;
    mean += d / n;
    m2 += d * (x - mean);
  }
  json result() const {
    return {{"count", n},
            {"sum", double(sum)},
            {"mean", n ? json(double(mean)) : json(nullptr)},
            {"m2", double(m2)},
            {"min", n ? json(min) : json(nullptr)},
            {"max", n ? json(max) : json(nullptr)}};
  }
};
void canceled(const std::atomic_bool *c) {
  if (c && c->load())
    throw std::runtime_error("canceled");
}
} // namespace
std::filesystem::path data_path(const std::filesystem::path &root, const std::string &name) {
  if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos ||
      name.find('\\') != std::string::npos)
    throw std::runtime_error("CSV requires a filename in the shared data directory");
  auto r = std::filesystem::canonical(root);
  auto p = std::filesystem::canonical(r / name);
  if (p.parent_path() != r || !std::filesystem::is_regular_file(p))
    throw std::runtime_error("invalid data path");
  return p;
}
std::vector<json> partition(const json &spec, const std::filesystem::path &root) {
  if (!spec.is_object())
    throw std::runtime_error("job must be an object");
  auto type = spec.at("type").get<std::string>();
  for (const auto &[key, value] : spec.items()) {
    (void)value;
    if (key != "type" && key != "partitions" &&
        !(type == "primes" && (key == "start" || key == "end")) &&
        !(type == "csv" && key == "file"))
      throw std::runtime_error("unsupported job field: " + key);
  }
  auto parts = spec.contains("partitions") ? integer(spec, "partitions", 1, 128) : 12;
  uint64_t start = 0, end = 0;
  std::string file;
  if (type == "primes") {
    start = integer(spec, "start", 0, 10000000000ULL);
    end = integer(spec, "end", 1, 10000000000ULL);
    if (end <= start || end - start > 20000000)
      throw std::runtime_error("prime range must be nonempty and <= 20 million");
  } else if (type == "csv") {
    file = spec.at("file").get<std::string>();
    end = std::filesystem::file_size(data_path(root, file));
    if (end > 64 * 1024 * 1024)
      throw std::runtime_error("CSV maximum is 64 MiB");
  } else
    throw std::runtime_error("supported types: primes, csv");
  std::vector<json> out;
  uint64_t length = end - start;
  parts = std::min(parts, std::max(uint64_t(1), length));
  for (uint64_t i = 0; i < parts; i++) {
    json t = {{"type", type},
              {"begin", start + length * i / parts},
              {"end", start + length * (i + 1) / parts}};
    if (type == "csv")
      t["file"] = file;
    out.push_back(t);
  }
  return out;
}
json compute(const json &task, const std::filesystem::path &root, const std::atomic_bool *cancel) {
  auto type = task.at("type").get<std::string>();
  uint64_t begin = task.at("begin"), end = task.at("end");
  if (end < begin)
    throw std::runtime_error("invalid task range");
  if (type == "primes") {
    uint64_t count = 0;
    for (auto n = begin; n < end; n++) {
      if ((n & 1023) == 0)
        canceled(cancel);
      if (n < 2)
        continue;
      bool prime = true;
      if (n != 2 && n % 2 == 0)
        continue;
      for (uint64_t d = 3; d <= n / d; d += 2)
        if (n % d == 0) {
          prime = false;
          break;
        }
      if (prime)
        count++;
    }
    return {{"count", count}};
  }
  if (type != "csv")
    throw std::runtime_error("unsupported task");
  std::ifstream in(data_path(root, task.at("file")));
  if (!in)
    throw std::runtime_error("cannot read CSV");
  if (begin) {
    in.seekg(begin - 1);
    char prev = 0;
    if (!in.get(prev))
      throw std::runtime_error("CSV changed or task range exceeds input");
    if (prev != '\n') {
      std::string skip;
      std::getline(in, skip);
    }
  }
  Stats s;
  std::string line;
  while (in && uint64_t(in.tellg()) < end) {
    canceled(cancel);
    if (!std::getline(in, line))
      break;
    if (line.empty())
      throw std::runtime_error("empty CSV row");
    size_t pos = 0;
    while (true) {
      canceled(cancel);
      auto comma = line.find(',', pos);
      auto value = line.substr(pos, comma == std::string::npos ? comma : comma - pos);
      size_t used = 0;
      double x = std::stod(value, &used);
      while (used < value.size() && std::isspace(static_cast<unsigned char>(value[used])))
        used++;
      if (used != value.size())
        throw std::runtime_error("malformed numeric CSV cell");
      s.add(x);
      if (comma == std::string::npos)
        break;
      pos = comma + 1;
    }
  }
  return s.result();
}
json aggregate(const std::string &type, const std::vector<json> &results) {
  if (type == "primes") {
    uint64_t n = 0;
    for (auto &r : results)
      n += r.at("count").get<uint64_t>();
    return {{"count", n}};
  }
  Stats s;
  for (auto &r : results) {
    uint64_t n = r.at("count");
    if (!n)
      continue;
    long double mean = r.at("mean").get<double>(), m2 = r.at("m2").get<double>();
    double low = r.at("min"), high = r.at("max");
    if (!s.n) {
      s.min = low;
      s.max = high;
    } else {
      s.min = std::min(s.min, low);
      s.max = std::max(s.max, high);
    }
    auto d = mean - s.mean;
    auto total = s.n + n;
    s.m2 += m2 + d * d * s.n * n / total;
    s.mean += d * n / total;
    s.n = total;
    s.sum += r.at("sum").get<double>();
  }
  auto r = s.result();
  r.erase("m2");
  r["variance"] = s.n ? json(double(s.m2 / s.n)) : json(nullptr);
  return r;
}
json serial_reference(const json &spec, const std::filesystem::path &root) {
  auto single = spec;
  single["partitions"] = 1;
  auto tasks = partition(single, root);
  return aggregate(spec.at("type"), {compute(tasks.at(0), root)});
}
} // namespace dce
