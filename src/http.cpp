#include "dce/http.hpp"
namespace dce {
Response response(unsigned status, const json &j) {
  Response r{static_cast<http::status>(status), 11};
  r.set(http::field::content_type, "application/json");
  r.body() = j.dump();
  r.prepare_payload();
  return r;
}
namespace {
struct Session : std::enable_shared_from_this<Session> {
  boost::beast::tcp_stream stream;
  boost::beast::flat_buffer buffer;
  http::request_parser<http::string_body> parser;
  Response output;
  Handler handle;
  Session(tcp::socket s, Handler h) : stream(std::move(s)), handle(std::move(h)) {
    parser.body_limit(65536);
    parser.header_limit(8192);
  }
  void start() {
    stream.expires_after(std::chrono::seconds(5));
    auto self = shared_from_this();
    http::async_read(stream, buffer, parser, [self](auto ec, size_t) {
      if (ec)
        return;
      try {
        self->output = self->handle(self->parser.get());
      } catch (const std::exception &e) {
        self->output = response(400, {{"error", e.what()}});
      }
      self->output.keep_alive(false);
      self->output.set("X-Content-Type-Options", "nosniff");
      self->stream.expires_after(std::chrono::seconds(5));
      http::async_write(self->stream, self->output, [self](auto, size_t) {
        boost::system::error_code ignored;
        self->stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
      });
    });
  }
};
struct Listener : std::enable_shared_from_this<Listener> {
  tcp::acceptor acceptor;
  Handler handler;
  Listener(net::io_context &io, std::string bind, unsigned short port, Handler h)
      : acceptor(io), handler(std::move(h)) {
    tcp::endpoint ep(net::ip::make_address(bind), port);
    acceptor.open(ep.protocol());
    acceptor.set_option(net::socket_base::reuse_address(true));
    acceptor.bind(ep);
    acceptor.listen();
  }
  void accept() {
    auto self = shared_from_this();
    acceptor.async_accept([self](auto ec, tcp::socket s) {
      if (!ec)
        std::make_shared<Session>(std::move(s), self->handler)->start();
      if (self->acceptor.is_open())
        self->accept();
    });
  }
};
} // namespace
void serve_http(net::io_context &io, const std::string &bind, unsigned short port, Handler h) {
  std::make_shared<Listener>(io, bind, port, std::move(h))->accept();
}
json request(const std::string &host, unsigned short port, http::verb method,
             const std::string &target, const json &body) {
  net::io_context io;
  tcp::resolver resolver(io);
  boost::beast::tcp_stream stream(io);
  http::request<http::string_body> req{method, target, 11};
  req.set(http::field::host, host);
  req.set(http::field::content_type, "application/json");
  if (!body.is_null())
    req.body() = body.dump();
  req.prepare_payload();
  boost::beast::flat_buffer buffer;
  http::response_parser<http::string_body> parser;
  parser.body_limit(32 * 1024 * 1024);
  std::string error;
  stream.expires_after(std::chrono::seconds(10));
  stream.async_connect(resolver.resolve(host, std::to_string(port)), [&](auto ec, auto) {
    if (ec) {
      error = ec.message();
      return;
    }
    http::async_write(stream, req, [&](auto ec, size_t) {
      if (ec) {
        error = ec.message();
        return;
      }
      http::async_read(stream, buffer, parser, [&](auto ec, size_t) {
        if (ec)
          error = ec.message();
      });
    });
  });
  io.run();
  if (!error.empty())
    throw std::runtime_error(error);
  auto &r = parser.get();
  auto j = json::parse(r.body());
  if (r.result_int() >= 400)
    throw std::runtime_error("HTTP " + std::to_string(r.result_int()) + ": " + j.dump());
  return j;
}
} // namespace dce
