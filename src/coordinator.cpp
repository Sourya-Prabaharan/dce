#include "dce/http.hpp"
#include "dce/protocol.hpp"
#include "dce/scheduler.hpp"
#include <fstream>
namespace dce {
class Coordinator {
  net::io_context &io_;
  Scheduler scheduler_;
  tcp::acceptor acceptor_;
  net::steady_timer timer_;
  bool stopping_ = false;
  std::string dashboard_;
  struct Peer {
    std::shared_ptr<Channel> channel;
    std::string session;
    Clock::time_point joined = Clock::now();
  };
  std::map<std::string, Peer> peers_;
  void dispatch() {
    if (stopping_)
      return;
    for (auto &a : scheduler_.dispatch()) {
      for (auto &[id, p] : peers_)
        if (p.session == a.session) {
          p.channel->send(a.message);
          break;
        }
    }
  }
  void accept() {
    acceptor_.async_accept([this](auto ec, tcp::socket s) {
      if (!ec) {
        if (peers_.size() >= 256) {
          boost::system::error_code ignored;
          s.close(ignored);
        } else {
          auto id = uid();
          auto ch = std::make_shared<Channel>(std::move(s));
          peers_.emplace(id, Peer{ch, ""});
          ch->on_message = [this, id](json m) {
            auto &p = peers_.at(id);
            auto type = m.at("type").get<std::string>();
            if (p.session.empty()) {
              if (type != "register" || m.at("version") != 1)
                throw std::runtime_error("register protocol version 1 first");
              p.session = scheduler_.add_worker(m.at("worker_id"), m.at("capacity"));
              p.channel->send(
                  {{"type", "registered"}, {"session_id", p.session}, {"heartbeat_ms", 500}});
            } else if (type == "heartbeat")
              scheduler_.heartbeat(p.session);
            else if (type == "ack")
              scheduler_.acknowledge(p.session, m);
            else if (type == "complete" || type == "failure")
              scheduler_.finish(p.session, m);
            else
              throw std::runtime_error("invalid message type");
            dispatch();
          };
          ch->on_close = [this, id] {
            auto i = peers_.find(id);
            if (i != peers_.end()) {
              scheduler_.disconnect(i->second.session);
              peers_.erase(i);
              dispatch();
            }
          };
          ch->start();
        }
      }
      if (!stopping_)
        accept();
    });
  }
  void tick() {
    timer_.expires_after(std::chrono::milliseconds(100));
    timer_.async_wait([this](auto ec) {
      if (ec || stopping_)
        return;
      auto dead = scheduler_.expire();
      std::vector<std::shared_ptr<Channel>> close;
      for (auto &[id, p] : peers_)
        if ((p.session.empty() && Clock::now() - p.joined > std::chrono::seconds(3)) ||
            std::find(dead.begin(), dead.end(), p.session) != dead.end())
          close.push_back(p.channel);
      for (auto &c : close)
        c->close();
      dispatch();
      tick();
    });
  }
  Response api(const Request &r) {
    std::string path(r.target());
    if (r.method() == http::verb::get) {
      if (path == "/") {
        Response out{http::status::ok, 11};
        out.set(http::field::content_type, "text/html; charset=utf-8");
        out.body() = dashboard_;
        out.prepare_payload();
        return out;
      }
      if (path == "/health")
        return response(200, {{"status", "ok"}, {"protocol_version", 1}});
      if (path == "/workers")
        return response(200, scheduler_.workers());
      if (path == "/events")
        return response(200, scheduler_.events());
      if (path == "/jobs")
        return response(200, scheduler_.jobs());
      if (path.starts_with("/jobs/")) {
        auto id = path.substr(6);
        bool result = id.ends_with("/result");
        if (result)
          id.resize(id.size() - 7);
        try {
          auto j = scheduler_.job(id);
          if (result) {
            if (j["state"] != "completed")
              return response(409, {{"error", "job not completed"}, {"state", j["state"]}});
            return response(200, j["result"]);
          }
          return response(200, j);
        } catch (const std::out_of_range &) {
          return response(404, {{"error", "unknown job"}});
        }
      }
    }
    if (path == "/jobs" && r.method() == http::verb::post) {
      auto id = scheduler_.submit(json::parse(r.body()));
      dispatch();
      return response(201, {{"id", id}});
    }
    return response(404, {{"error", "unknown endpoint"}});
  }

public:
  Coordinator(net::io_context &io, const Args &a)
      : io_(io), scheduler_(a.get("data", "data"), a.number("attempts", 3, 1, 10),
                            a.number("timeout-ms", 2000, 1000, 30000),
                            a.number("lease-ms", 60000, 2000, 600000)),
        acceptor_(io), timer_(io) {
    std::ifstream file(a.get("dashboard", "web/index.html"));
    if (!file)
      throw std::runtime_error("dashboard missing; pass --dashboard path");
    dashboard_ = {std::istreambuf_iterator<char>(file), {}};
    auto bind = a.get("bind", "127.0.0.1");
    tcp::endpoint ep(net::ip::make_address(bind), a.number("port", 9000, 1, 65535));
    acceptor_.open(ep.protocol());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(ep);
    acceptor_.listen();
    serve_http(io_, bind, a.number("http-port", 8080, 1, 65535),
               [this](auto &r) { return api(r); });
    accept();
    tick();
    log("coordinator_ready", {{"bind", bind}});
  }
  void stop() {
    stopping_ = true;
    timer_.cancel();
    boost::system::error_code ec;
    acceptor_.close(ec);
    std::vector<std::shared_ptr<Channel>> all;
    for (auto &[id, p] : peers_)
      all.push_back(p.channel);
    for (auto &c : all)
      c->close();
    io_.stop();
  }
};
} // namespace dce
int main(int argc, char **argv) {
  try {
    dce::Args a(argc, argv);
    dce::net::io_context io;
    dce::Coordinator c(io, a);
    dce::net::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, int) { c.stop(); });
    io.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
