#pragma once

#include "auction_manager.h"
#include "logger.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

namespace AuctionWs {

class Session;

class Server {
public:
  Server(net::io_context &ioc, const tcp::endpoint &endpoint,
         Auction::AuctionManager &manager, Logging::Logger &logger);

  void Run();

  void RegisterSession(const std::shared_ptr<Session> &session);
  void UnregisterSession(const std::shared_ptr<Session> &session);
  void Broadcast(const json &message);

  Auction::AuctionManager &GetManager();
  Logging::Logger &GetLogger();

private:
  void DoAccept();
  void OnAccept(beast::error_code ec, tcp::socket socket);

private:
  net::io_context &ioc_;
  tcp::acceptor acceptor_;
  Auction::AuctionManager &manager_;
  Logging::Logger &logger_;

  std::mutex sessions_mutex_;
  std::unordered_set<std::shared_ptr<Session>> sessions_;
};

} // namespace AuctionWs
