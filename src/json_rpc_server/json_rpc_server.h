// json_rpc_server.hpp
#pragma once

#include "logger.h"
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

// Forward declarations
class MethodRegistry;
class Server;
class Session;

// Типы для обработчиков
using MethodHandler = std::function<json(const json &, const std::string &)>;
using ErrorHandler =
    std::function<json(int, const std::string &, const std::string &)>;

// Структура для описания метода
struct MethodInfo {
  std::string name;
  std::string description;
  json params_schema;
  json result_schema;
  MethodHandler handler;
  std::vector<int> allowed_errors;
};

// Структура для ошибок JSON-RPC
struct JsonRpcError {
  int code;
  std::string message;
  json data;

  JsonRpcError(int c, const std::string &m, const json &d = nullptr)
      : code(c), message(m), data(d) {}

  json to_json() const {
    json error = {{"code", code}, {"message", message}};
    if (!data.is_null()) {
      error["data"] = data;
    }
    return error;
  }
};

// Стандартные ошибки JSON-RPC
namespace Errors {
const JsonRpcError ParseError(-32700, "Parse error");
const JsonRpcError InvalidRequest(-32600, "Invalid Request");
const JsonRpcError MethodNotFound(-32601, "Method not found");
const JsonRpcError InvalidParams(-32602, "Invalid params");
const JsonRpcError InternalError(-32603, "Internal error");
} // namespace Errors

// Реестр методов
class MethodRegistry {
public:
  MethodRegistry();

  void RegisterMethod(const MethodInfo &method);

  bool HasMethod(const std::string &name) const;

  MethodInfo GetMethod(const std::string &name) const;

  void SetErrorMessage(int code, const std::string &language,
                       const std::string &message);

  std::string GetErrorMessage(int code, const std::string &language) const;

  std::vector<std::string> GetMethodNames() const;

private:
  std::unordered_map<std::string, MethodInfo> methods_;
  std::unordered_map<std::string, std::string> method_descriptions_;
  std::unordered_map<std::string, std::string> error_messages_;
  mutable std::mutex mutex_;
  std::string default_language_ = "en-US";
};

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

// Основной сервер
class Server {
public:
  Server(net::io_context &ioc, const tcp::endpoint &endpoint,
         const std::string &version, Logging::Logger &logger);

  MethodRegistry &get_registry();

  void Run();

private:
  void do_accept();

  void onAccept(beast::error_code ec, tcp::socket socket);

private:
  net::io_context &ioc_;
  tcp::acceptor acceptor_;
  MethodRegistry registry_;
  std::string version_;
  Logging::Logger &logger_;
};

} // namespace JsonRpc
