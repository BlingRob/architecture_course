#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct StoredEvent {
  std::uint64_t id{};
  std::string event;
  boost::json::value payload;
  std::int64_t received_at_ms{};
};

class EventStore {
public:
  explicit EventStore(std::size_t max_events) : max_events_(max_events) {}

  std::uint64_t Add(std::string event, boost::json::value payload) {
    std::lock_guard<std::mutex> lock(mutex_);
    StoredEvent stored;
    stored.id = next_id_++;
    stored.event = std::move(event);
    stored.payload = std::move(payload);
    stored.received_at_ms = NowMs();

    events_.push_back(std::move(stored));
    while (events_.size() > max_events_) {
      events_.pop_front();
    }

    return next_id_ - 1;
  }

  boost::json::array List() const {
    std::lock_guard<std::mutex> lock(mutex_);
    boost::json::array result;
    result.reserve(events_.size());

    for (const auto &e : events_) {
      result.push_back({{"id", e.id},
                        {"event", e.event},
                        {"receivedAtMs", e.received_at_ms},
                        {"payload", e.payload}});
    }

    return result;
  }

private:
  static std::int64_t NowMs() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
        .count();
  }

private:
  mutable std::mutex mutex_;
  std::deque<StoredEvent> events_;
  std::size_t max_events_;
  std::uint64_t next_id_{1};
};

http::response<http::string_body>
MakeJsonResponse(http::status status, const boost::json::value &body,
                 unsigned version, bool keep_alive) {
  http::response<http::string_body> res{status, version};
  res.set(http::field::content_type, "application/json");
  res.keep_alive(keep_alive);
  res.body() = boost::json::serialize(body);
  res.prepare_payload();
  return res;
}

http::response<http::string_body>
HandleRequest(const http::request<http::string_body> &req, EventStore &store,
              const std::string &expected_secret) {
  const auto target = std::string(req.target());

  if (req.method() == http::verb::get && target == "/health") {
    return MakeJsonResponse(http::status::ok, { {"status", "ok"} }, req.version(),
                            req.keep_alive());
  }

  if (req.method() == http::verb::get && target == "/webhooks/events") {
    return MakeJsonResponse(
        http::status::ok,
        {{"count", static_cast<std::uint64_t>(store.List().size())},
         {"events", store.List()}},
        req.version(), req.keep_alive());
  }

  if (req.method() == http::verb::post && target == "/webhooks/incoming") {
    if (!expected_secret.empty()) {
      const auto secret_it = req.find("X-Webhook-Secret");
      const std::string provided =
          secret_it != req.end() ? std::string(secret_it->value()) : "";
      if (provided != expected_secret) {
        return MakeJsonResponse(
            http::status::unauthorized,
            {{"ok", false}, {"error", "invalid webhook secret"}},
            req.version(), req.keep_alive());
      }
    }

    boost::json::value payload;
    try {
      payload = boost::json::parse(req.body());
    } catch (const std::exception &) {
      return MakeJsonResponse(
          http::status::bad_request,
          {{"ok", false}, {"error", "body must be valid JSON"}},
          req.version(), req.keep_alive());
    }

    const auto event_it = req.find("X-Webhook-Event");
    const std::string event_name =
        event_it != req.end() ? std::string(event_it->value()) : "unknown";

    const auto id = store.Add(event_name, payload);

    return MakeJsonResponse(http::status::accepted,
                            {{"ok", true}, {"receivedId", id}},
                            req.version(), req.keep_alive());
  }

  return MakeJsonResponse(http::status::not_found,
                          {{"ok", false}, {"error", "route not found"}},
                          req.version(), req.keep_alive());
}

void DoSession(tcp::socket socket, EventStore &store,
               const std::string &expected_secret) {
  beast::flat_buffer buffer;
  beast::error_code ec;

  http::request<http::string_body> req;
  http::read(socket, buffer, req, ec);
  if (ec) {
    return;
  }

  auto res = HandleRequest(req, store, expected_secret);
  http::write(socket, res, ec);

  socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
  try {
    const std::string host = "0.0.0.0";
    const unsigned short port = 19090;

    const char *secret_env = std::getenv("WEBHOOK_SECRET");
    const std::string expected_secret = secret_env ? secret_env : "dev-secret";

    net::io_context ioc{1};
    tcp::acceptor acceptor{ioc, {net::ip::make_address(host), port}};
    EventStore store(100);

    std::cout << "Webhook example server listening on http://" << host << ":"
              << port << "\n";
    std::cout << "Routes: POST /webhooks/incoming, GET /webhooks/events, GET /health\n";
    std::cout << "Webhook secret (X-Webhook-Secret): " << expected_secret << "\n";

    while (true) {
      tcp::socket socket{ioc};
      acceptor.accept(socket);
      DoSession(std::move(socket), store, expected_secret);
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
