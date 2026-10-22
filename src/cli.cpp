#include "dce/compute.hpp"
#include "dce/http.hpp"
#include <fstream>
#include <thread>
int main(int argc, char **argv) {
  using namespace dce;
  try {
    if (argc < 2)
      throw std::runtime_error("Usage: dce-cli "
                               "{submit|status|result|jobs|workers|health|events|wait|demo|serial|"
                               "generate} [--option value]");
    std::string cmd = argv[1];
    Args a(argc - 1, argv + 1);
    auto host = a.get("host", "127.0.0.1");
    auto port = a.number("port", 8080, 1, 65535);
    auto spec = [&] {
      if (a.opts.contains("json"))
        return json::parse(a.get("json", ""));
      auto type = a.get("type", "primes");
      json j = {{"type", type}, {"partitions", a.number("partitions", 12, 1, 128)}};
      if (type == "csv")
        j["file"] = a.get("file", "numbers.csv");
      else {
        j["start"] = std::stoull(a.get("start", "0"));
        j["end"] = std::stoull(a.get("end", "1000000"));
      }
      return j;
    };
    if (cmd == "generate") {
      auto path = a.get("output", "data/numbers.csv");
      std::ofstream out(path);
      if (!out)
        throw std::runtime_error("cannot open output");
      int rows = a.number("rows", 10000, 1, 1000000);
      for (int i = 0; i < rows; i++)
        out << i % 1000 - 500 << ',' << (i * 17) % 1000 - 500 << '\n';
      std::cout << json({{"file", path}, {"rows", rows}}).dump() << '\n';
      return 0;
    }
    if (cmd == "serial") {
      std::cout << serial_reference(spec(), a.get("data", "data")).dump(2) << '\n';
      return 0;
    }
    json out;
    auto get = [&](std::string p) { return request(host, port, http::verb::get, p); };
    if (cmd == "submit")
      out = request(host, port, http::verb::post, "/jobs", spec());
    else if (cmd == "status")
      out = get("/jobs/" + a.get("id", ""));
    else if (cmd == "result")
      out = get("/jobs/" + a.get("id", "") + "/result");
    else if (cmd == "jobs" || cmd == "workers" || cmd == "health" || cmd == "events")
      out = get("/" + cmd);
    else if (cmd == "wait" || cmd == "demo") {
      std::string id =
          cmd == "demo"
              ? request(host, port, http::verb::post, "/jobs", spec()).at("id").get<std::string>()
              : a.get("id", "");
      auto until = Clock::now() + std::chrono::seconds(a.number("timeout", 120, 1, 3600));
      while (true) {
        out = get("/jobs/" + id);
        if (out["state"] == "completed")
          break;
        if (out["state"] == "failed")
          throw std::runtime_error(out.dump());
        if (Clock::now() > until)
          throw std::runtime_error("wait timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      }
    } else
      throw std::runtime_error("unknown command");
    std::cout << out.dump(2) << '\n';
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
