// json_rpc_server.hpp
#pragma once

#include "logger.h"
#include "method_registry.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

namespace JsonRpc {

// Обработчик сессии WebSocket
class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket &&socket, MethodRegistry &registry,
          const std::string &version, Logging::Logger &logger);

  void Run();

private:
  void onAccept(beast::error_code ec);

  void doRead();

  void safeWrite(const json &response);

  void onRead(beast::error_code ec, std::size_t bytes_transferred);

  json processRequest(const json &request);

  json processBatchRequest(const json &requests);

  json createErrorResponse(const json &id, const JsonRpcError &error,
                           const std::string &language);

  json createErrorResponse(const json &id, int code, const std::string &message,
                           const json &data = json());

  json createErrorResponse(const json &id, int code, const char *message,
                           const json &data = json());

  void sendErrorResponse(const json &id, const JsonRpcError &error,
                         const std::string &language);

  void onWrite(beast::error_code ec, std::size_t bytes_transferred);

private:
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  beast::flat_buffer write_buffer_;
  MethodRegistry &registry_;
  std::string version_;
  Logging::Logger &logger_;
};

} // namespace JsonRpc
