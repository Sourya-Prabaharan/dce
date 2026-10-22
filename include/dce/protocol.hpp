#pragma once
#include "common.hpp"
#include <deque>
#include <functional>
namespace dce {
constexpr size_t max_message = 1024 * 1024;
std::string frame(const json &);
uint32_t frame_size(const std::array<unsigned char, 4> &);
// All Channel methods execute on its owning io_context thread.
class Channel : public std::enable_shared_from_this<Channel> {
  tcp::socket socket_;
  std::array<unsigned char, 4> header_{};
  std::string body_;
  std::deque<std::string> writes_;
  bool closed_ = false;
  void read_header();
  void read_body();
  void write_next();

public:
  std::function<void(json)> on_message;
  std::function<void()> on_close;
  explicit Channel(tcp::socket socket) : socket_(std::move(socket)) {}
  void start();
  void send(const json &);
  void close();
};
} // namespace dce
