#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

std::int64_t NowMs() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

void WriteSimpleResponse(tcp::socket &socket,
                         http::status status,
                         const std::string &content_type,
                         const std::string &body,
                         unsigned version,
                         bool keep_alive,
                         beast::error_code &ec) {
  http::response<http::string_body> res{status, version};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, content_type);
  res.keep_alive(keep_alive);
  res.body() = body;
  res.prepare_payload();
  http::write(socket, res, ec);
}

void StreamSse(tcp::socket &socket, unsigned version, bool keep_alive,
               beast::error_code &ec) {
  (void)version;
  (void)keep_alive;

  const std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/event-stream\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: keep-alive\r\n"
      "X-Accel-Buffering: no\r\n"
      "\r\n";

  net::write(socket, net::buffer(headers), ec);
  if (ec) {
    return;
  }

  const std::string retry = "retry: 3000\n\n";
  net::write(socket, net::buffer(retry), ec);
  if (ec) {
    return;
  }

  for (int i = 1; i <= 10; ++i) {
    const std::string event =
        "event: tick\n"
        "id: " +
        std::to_string(i) + "\n" +
        "data: {\"counter\":" + std::to_string(i) +
        ",\"timestampMs\":" + std::to_string(NowMs()) + "}\n\n";

    net::write(socket, net::buffer(event), ec);
    if (ec) {
      return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

}

void DoSession(tcp::socket socket) {
  beast::error_code ec;
  beast::flat_buffer buffer;

  http::request<http::string_body> req;
  http::read(socket, buffer, req, ec);
  if (ec) {
    return;
  }

  const std::string target = std::string(req.target());

  if (req.method() == http::verb::get && target == "/health") {
    WriteSimpleResponse(socket, http::status::ok, "application/json",
                        "{\"status\":\"ok\"}", req.version(),
                        req.keep_alive(), ec);
  } else if (req.method() == http::verb::get && target == "/events") {
    StreamSse(socket, req.version(), req.keep_alive(), ec);
  } else {
    WriteSimpleResponse(socket, http::status::not_found, "application/json",
                        "{\"error\":\"route not found\"}", req.version(),
                        req.keep_alive(), ec);
  }

  socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
  try {
    const std::string host = "0.0.0.0";
    const unsigned short port = 19191;

    net::io_context ioc{1};
    tcp::acceptor acceptor{ioc, {net::ip::make_address(host), port}};

    std::cout << "SSE example server listening on http://" << host << ":"
              << port << "\n";
    std::cout << "Routes: GET /events, GET /health\n";

    while (true) {
      tcp::socket socket{ioc};
      acceptor.accept(socket);
      DoSession(std::move(socket));
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
