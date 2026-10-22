#include "dce/protocol.hpp"
#include "dce/scheduler.hpp"
#include <cmath>
#include <fstream>
#include <thread>
using namespace dce;
void check(bool ok, const char *msg) {
  if (!ok)
    throw std::runtime_error(msg);
}
template <class F> void rejects(F f) {
  bool threw = false;
  try {
    f();
  } catch (...) {
    threw = true;
  }
  check(threw, "expected rejection");
}
json result(const Assignment &a, uint64_t count = 1) {
  return {{"type", "complete"},
          {"task_id", a.message.at("task_id")},
          {"attempt_id", a.message.at("attempt_id")},
          {"result", {{"count", count}}}};
}
int main() {
  try {
    auto root = std::filesystem::temp_directory_path() / ("dce-test-" + uid());
    std::filesystem::create_directories(root);
    {
      std::ofstream f(root / "a.csv");
      f << "1,2,3\n4,5\n6,7,8,9,10\n";
    }
    json csv = {{"type", "csv"}, {"file", "a.csv"}, {"partitions", 13}};
    std::vector<json> parts;
    for (auto &t : partition(csv, root))
      parts.push_back(compute(t, root));
    auto stats = aggregate("csv", parts);
    check(stats["count"] == 10 && stats["sum"] == 55 && stats["mean"] == 5.5 &&
              std::abs(stats["variance"].get<double>() - 8.25) < 1e-12,
          "CSV aggregation");
    check(stats == serial_reference(csv, root), "serial CSV reference");
    {
      std::ofstream f(root / "stable.csv");
      f << "1000000000001\n1000000000002\n1000000000003\n";
    }
    auto stable = serial_reference({{"type", "csv"}, {"file", "stable.csv"}}, root);
    check(std::abs(stable["variance"].get<double>() - 2.0 / 3) < 1e-12, "stable variance");
    {
      std::ofstream f(root / "bad.csv");
      f << "1,nope\n";
    }
    rejects([&] { serial_reference({{"type", "csv"}, {"file", "bad.csv"}}, root); });
    rejects([&] { data_path(root, "../secret"); });
    json prime = {{"type", "primes"}, {"start", 0}, {"end", 100000}, {"partitions", 12}};
    check(serial_reference(prime, root)["count"] == 9592, "known prime count");
    parts.clear();
    for (auto &t : partition(prime, root))
      parts.push_back(compute(t, root));
    check(aggregate("primes", parts)["count"] == 9592, "partition primes");
    rejects([&] { partition({{"type", "shell"}}, root); });
    rejects([&] { partition({{"type", "primes"}, {"start", -1}, {"end", 50}}, root); });
    Scheduler s(root);
    auto now = Clock::now();
    auto w1 = s.add_worker("one", 1, now), w3 = s.add_worker("three", 3, now);
    auto job = s.submit(prime, now);
    auto assigned = s.dispatch(now);
    check(assigned.size() == 4, "capacity bound");
    size_t one = 0, three = 0;
    for (auto &a : assigned) {
      one += a.session == w1;
      three += a.session == w3;
    }
    check(one == 1 && three == 3, "normalized load selection");
    std::map<std::string, int> loads;
    for (const auto &assignment : assigned) {
      double chosen = double(loads[assignment.session]) / (assignment.session == w1 ? 1 : 3);
      check(chosen <= loads[w1] && chosen <= double(loads[w3]) / 3,
            "each dispatch chooses the least loaded capacity ratio");
      loads[assignment.session]++;
    }
    check(s.dispatch(now).empty(), "no overdispatch");
    for (auto &a : assigned)
      check(s.finish(a.session, result(a), now), "accept completion");
    check(!s.finish(assigned[0].session, result(assigned[0]), now), "duplicate ignored");
    check(s.dispatch(now).size() == 4, "release slots");
    Scheduler recovery(root, 2);
    auto old = recovery.add_worker("restart", 1, now);
    auto j =
        recovery.submit({{"type", "primes"}, {"start", 0}, {"end", 10}, {"partitions", 1}}, now);
    auto a = recovery.dispatch(now).at(0);
    recovery.acknowledge(old, a.message);
    auto dead = recovery.expire(now + std::chrono::milliseconds(2100));
    check(dead.size() == 1, "heartbeat timeout");
    auto fresh = recovery.add_worker("restart", 1, now + std::chrono::milliseconds(2200));
    auto b = recovery.dispatch(now + std::chrono::milliseconds(2200)).at(0);
    check(fresh != old && a.message["attempt_id"] != b.message["attempt_id"],
          "new session attempt");
    check(!recovery.finish(old, result(a)), "stale attempt rejected");
    check(recovery.finish(fresh, result(b, 4)), "recovery accepted");
    check(recovery.job(j)["result"]["count"] == 4, "recovery result");
    Scheduler exhaustion(root, 2);
    auto e = exhaustion.add_worker("bad", 1);
    auto ej = exhaustion.submit({{"type", "primes"}, {"start", 0}, {"end", 5}, {"partitions", 1}});
    for (int i = 0; i < 2; i++) {
      auto x = exhaustion.dispatch().at(0);
      auto m = result(x);
      m["type"] = "failure";
      m["error"] = "bad input";
      exhaustion.finish(e, m);
    }
    check(exhaustion.job(ej)["state"] == "failed" && exhaustion.dispatch().empty(),
          "bounded retries");
    Scheduler ack(root);
    auto aw = ack.add_worker("silent", 1, now);
    ack.submit(prime, now);
    ack.dispatch(now);
    ack.heartbeat(aw, now + std::chrono::milliseconds(1100));
    check(ack.expire(now + std::chrono::milliseconds(1100)).size() == 1, "ACK timeout");
    Scheduler lease(root, 3, 2000, 3000);
    auto lw = lease.add_worker("stuck", 1, now);
    lease.submit(prime, now);
    auto la = lease.dispatch(now).at(0);
    lease.acknowledge(lw, la.message);
    lease.heartbeat(lw, now + std::chrono::milliseconds(3100));
    check(lease.expire(now + std::chrono::milliseconds(3100)).size() == 1, "execution lease");
    Scheduler malformed(root);
    auto mw = malformed.add_worker("malformed", 1);
    malformed.submit(prime);
    auto ma = malformed.dispatch().at(0);
    auto invalid = result(ma);
    invalid["type"] = "failure";
    invalid["error"] = 42;
    rejects([&] { malformed.finish(mw, invalid); });
    malformed.disconnect(mw);
    malformed.add_worker("healthy", 1);
    check(!malformed.dispatch().empty(), "malformed results do not orphan tasks");
    Scheduler concurrent(root);
    std::vector<std::thread> submitters;
    for (int i = 0; i < 8; i++)
      submitters.emplace_back([&] {
        for (int j = 0; j < 15; j++)
          concurrent.submit(prime);
      });
    for (auto &t : submitters)
      t.join();
    check(concurrent.jobs().size() == 120, "concurrent queue");
    std::array<unsigned char, 4> zero{};
    rejects([&] { frame_size(zero); });
    std::array<unsigned char, 4> oversized{0, 16, 0, 1};
    rejects([&] { frame_size(oversized); });
    rejects([&] { frame(json(std::string(max_message, 'x'))); });
    net::io_context io;
    tcp::acceptor listener(io, {tcp::v4(), 0});
    std::atomic<int> received{0};
    std::shared_ptr<Channel> channel;
    listener.async_accept([&](auto ec, tcp::socket socket) {
      check(!ec, "accept");
      channel = std::make_shared<Channel>(std::move(socket));
      channel->on_message = [&](json m) {
        check(m["hello"] == "world", "fragmented payload");
        received++;
      };
      channel->start();
    });
    std::thread network([&] { io.run(); });
    net::io_context client_io;
    tcp::socket client(client_io);
    client.connect({net::ip::make_address("127.0.0.1"), listener.local_endpoint().port()});
    auto bytes = frame({{"hello", "world"}});
    for (char c : bytes)
      net::write(client, net::buffer(&c, 1));
    auto joined = bytes + bytes;
    net::write(client, net::buffer(joined));
    client.close();
    network.join();
    check(received == 3, "fragmented and coalesced frames");
    // Exercise parser rejection and mid-frame EOF through actual sockets.
    for (const auto &bad :
         {std::string("\0\0\0\0", 4), std::string("\0\20\0\1", 4), std::string("\0\0\0\1{", 5),
          std::string("\0\0", 2), std::string("\0\0\0\5xx", 6), frame(json::array())}) {
      net::io_context bad_io;
      tcp::acceptor acceptor(bad_io, {tcp::v4(), 0});
      bool closed = false;
      bool delivered = false;
      acceptor.async_accept([&](auto, tcp::socket socket) {
        auto peer = std::make_shared<Channel>(std::move(socket));
        peer->on_message = [&](json) { delivered = true; };
        peer->on_close = [&] { closed = true; };
        peer->start();
      });
      std::thread server([&] { bad_io.run(); });
      tcp::socket sender(client_io);
      sender.connect({net::ip::make_address("127.0.0.1"), acceptor.local_endpoint().port()});
      net::write(sender, net::buffer(bad));
      sender.close();
      server.join();
      check(closed && !delivered, "bad or truncated frames close without delivery");
    }
    std::filesystem::remove_all(root);
    std::cout << "PASS: framing, aggregation, validation, capacity, load selection, concurrent "
                 "queue, heartbeat, ACK timeout, reconnect sessions, stale/duplicate acceptance, "
                 "retry exhaustion\n";
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
