#include "dce/protocol.hpp"
namespace dce {
uint32_t frame_size(const std::array<unsigned char, 4> &h) {
  uint32_t n = (uint32_t(h[0]) << 24) | (uint32_t(h[1]) << 16) | (uint32_t(h[2]) << 8) | h[3];
  if (n == 0 || n > max_message)
    throw std::runtime_error("invalid frame length");
  return n;
}
std::string frame(const json &j) {
  auto s = j.dump();
  if (s.empty() || s.size() > max_message)
    throw std::runtime_error("message too large");
  uint32_t n = s.size();
  std::string r(4, '\0');
  for (int i = 0; i < 4; i++)
    r[i] = char(n >> (24 - 8 * i));
  return r + s;
}
void Channel::start() {
  read_header();
}
void Channel::read_header() {
  auto self = shared_from_this();
  net::async_read(socket_, net::buffer(header_), [self](auto ec, size_t) {
    if (ec) {
      self->close();
      return;
    }
    try {
      self->body_.resize(frame_size(self->header_));
      self->read_body();
    } catch (...) {
      self->close();
    }
  });
}
void Channel::read_body() {
  auto self = shared_from_this();
  net::async_read(socket_, net::buffer(body_), [self](auto ec, size_t) {
    if (ec) {
      self->close();
      return;
    }
    try {
      auto j = json::parse(self->body_);
      if (!j.is_object())
        throw std::runtime_error("object required");
      if (self->on_message)
        self->on_message(std::move(j));
      if (!self->closed_)
        self->read_header();
    } catch (const std::exception &e) {
      log("protocol_error", {{"error", e.what()}});
      self->close();
    }
  });
}
void Channel::send(const json &j) {
  if (closed_)
    return;
  if (writes_.size() >= 256) {
    close();
    return;
  }
  bool idle = writes_.empty();
  writes_.push_back(frame(j));
  if (idle)
    write_next();
}
void Channel::write_next() {
  auto self = shared_from_this();
  net::async_write(socket_, net::buffer(writes_.front()), [self](auto ec, size_t) {
    if (ec) {
      self->close();
      return;
    }
    self->writes_.pop_front();
    if (!self->writes_.empty())
      self->write_next();
  });
}
void Channel::close() {
  if (closed_)
    return;
  closed_ = true;
  boost::system::error_code ec;
  socket_.cancel(ec);
  socket_.close(ec);
  if (on_close)
    on_close(); /* Keep pending buffers alive until canceled handlers return. */
}
} // namespace dce
