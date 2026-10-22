#include "dce/compute.hpp"
#include "dce/protocol.hpp"
namespace dce {
class Worker {
  net::io_context &io_;
  net::thread_pool pool_;
  tcp::resolver resolver_;
  net::steady_timer reconnect_, heartbeat_, connect_timeout_;
  std::shared_ptr<Channel> channel_;
  std::shared_ptr<tcp::socket> connecting_;
  std::shared_ptr<std::atomic_bool> cancel_;
  std::string host_, port_, id_, root_;
  int capacity_, active_ = 0, backoff_ = 100;
  bool stopping_ = false;
  void retry() {
    if (stopping_)
      return;
    reconnect_.expires_after(std::chrono::milliseconds(backoff_));
    backoff_ = std::min(3000, backoff_ * 2);
    reconnect_.async_wait([this](auto ec) {
      if (!ec) {
        if (active_)
          retry();
        else
          connect();
      }
    });
  }
  void beat() {
    if (stopping_ || !channel_)
      return;
    channel_->send({{"type", "heartbeat"}});
    heartbeat_.expires_after(std::chrono::milliseconds(500));
    heartbeat_.async_wait([this](auto ec) {
      if (!ec)
        beat();
    });
  }
  void message(json m) {
    auto type = m.at("type").get<std::string>();
    if (type == "registered") {
      backoff_ = 100;
      beat();
      log("worker_registered", {{"worker", id_}, {"session", m.at("session_id")}});
      return;
    }
    if (type != "assign")
      throw std::runtime_error("unexpected coordinator message");
    if (active_ >= capacity_)
      throw std::runtime_error("capacity exceeded");
    auto task = m.at("task_id").get<std::string>();
    auto attempt = m.at("attempt_id").get<std::string>();
    auto payload = m.at("payload");
    channel_->send({{"type", "ack"}, {"task_id", task}, {"attempt_id", attempt}});
    active_++;
    auto token = cancel_;
    auto connection = channel_;
    net::post(pool_, [this, task, attempt, payload, token, connection] {
      json answer = {{"type", "complete"}, {"task_id", task}, {"attempt_id", attempt}};
      try {
        answer["result"] = compute(payload, root_, token.get());
      } catch (const std::exception &e) {
        answer["type"] = "failure";
        answer["error"] = e.what();
      }
      net::post(io_, [this, answer = std::move(answer), token, connection] {
        active_--;
        if (!stopping_ && !token->load() && channel_ == connection)
          connection->send(answer);
      });
    });
  }
  void connect() {
    if (stopping_)
      return;
    connecting_ = std::make_shared<tcp::socket>(io_);
    auto socket = connecting_;
    connect_timeout_.expires_after(std::chrono::seconds(5));
    connect_timeout_.async_wait([this, socket](auto ec) {
      if (!ec) {
        resolver_.cancel();
        boost::system::error_code ignored;
        socket->close(ignored);
      }
    });
    resolver_.async_resolve(host_, port_, [this, socket](auto ec, auto endpoints) {
      if (ec) {
        connect_timeout_.cancel();
        retry();
        return;
      }
      net::async_connect(*socket, endpoints, [this, socket](auto ec, auto) {
        connect_timeout_.cancel();
        if (stopping_)
          return;
        if (ec) {
          retry();
          return;
        }
        channel_ = std::make_shared<Channel>(std::move(*socket));
        cancel_ = std::make_shared<std::atomic_bool>(false);
        channel_->on_message = [this](json m) { message(std::move(m)); };
        channel_->on_close = [this] {
          cancel_->store(true);
          heartbeat_.cancel();
          channel_.reset();
          log("worker_disconnected", {{"worker", id_}});
          retry();
        };
        channel_->start();
        channel_->send(
            {{"type", "register"}, {"version", 1}, {"worker_id", id_}, {"capacity", capacity_}});
      });
    });
  }

public:
  Worker(net::io_context &io, const Args &a)
      : io_(io), pool_(a.number("threads", 1, 1, 64)), resolver_(io), reconnect_(io),
        heartbeat_(io), connect_timeout_(io), host_(a.get("host", "127.0.0.1")),
        port_(a.get("port", "9000")), id_(a.get("id", uid())), root_(a.get("data", "data")),
        capacity_(a.number("threads", 1, 1, 64)) {
    connect();
  }
  void stop() {
    stopping_ = true;
    if (cancel_)
      cancel_->store(true);
    resolver_.cancel();
    reconnect_.cancel();
    heartbeat_.cancel();
    connect_timeout_.cancel();
    if (connecting_) {
      boost::system::error_code ec;
      connecting_->close(ec);
    }
    if (channel_)
      channel_->close();
    pool_.join();
    io_.stop();
  }
};
} // namespace dce
int main(int argc, char **argv) {
  try {
    dce::Args a(argc, argv);
    dce::net::io_context io;
    dce::Worker w(io, a);
    dce::net::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](auto, int) { w.stop(); });
    io.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
