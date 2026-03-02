#pragma once

#include "server.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

namespace AuctionWs {

class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket &&socket, Server &server);

  void Run();
  void Deliver(const json &message);

private:
  void OnAccept(beast::error_code ec);
  void DoRead();
  void OnRead(beast::error_code ec, std::size_t bytes_transferred);

  void QueueWrite(std::string payload);
  void DoWrite();
  void OnWrite(beast::error_code ec, std::size_t bytes_transferred);

  json HandleRequest(const json &request);
  json HandleAction(const std::string &action, const json &request);

  json BuildSuccess(const std::string &action, const json &payload = json::object());
  json BuildError(const std::string &action, const std::string &error_message);

  static json UserToJson(const Auction::User &user);
  static json ItemToJson(const Auction::Item &item);
  static std::string StatusToString(Auction::ItemStatus status);

private:
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  Server &server_;
  std::deque<std::string> write_queue_;
};

} // namespace AuctionWs
