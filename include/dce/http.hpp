#pragma once
#include "common.hpp"
#include <boost/beast.hpp>
namespace dce {
namespace http = boost::beast::http;
using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;
using Handler = std::function<Response(const Request &)>;
Response response(unsigned status, const json &);
void serve_http(net::io_context &, const std::string &bind, unsigned short port, Handler);
json request(const std::string &host, unsigned short port, http::verb method,
             const std::string &target, const json &body = nullptr);
} // namespace dce
