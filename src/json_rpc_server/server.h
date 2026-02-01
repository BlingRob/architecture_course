// json_rpc_server.hpp
#pragma once

#include "logger.h"
#include "method_registry.h"
#include "session.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

namespace JsonRpc {

// Типы для обработчиков
using MethodHandler = std::function<json(const json &, const std::string &)>;

// Основной сервер
class Server {
public:
  Server(net::io_context &ioc, const tcp::endpoint &endpoint,
         const std::string &version, Logging::Logger &logger);

  MethodRegistry &GetRegistry();

  void Run();

private:
  void doAccept();

  void onAccept(beast::error_code ec, tcp::socket socket);

private:
  net::io_context &ioc_;
  tcp::acceptor acceptor_;
  MethodRegistry registry_;
  std::string version_;
  Logging::Logger &logger_;
};

} // namespace JsonRpc
