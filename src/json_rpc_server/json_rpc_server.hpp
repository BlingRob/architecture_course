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
private:
  std::unordered_map<std::string, MethodInfo> methods_;
  std::unordered_map<std::string, std::string> method_descriptions_;
  std::unordered_map<std::string, std::string> error_messages_;
  mutable std::mutex mutex_;
  std::string default_language_ = "en-US";

public:
  MethodRegistry() {
    // Инициализация стандартных ошибок
    set_error_message(Errors::ParseError.code, "en-US", "Parse error");
    set_error_message(Errors::InvalidRequest.code, "en-US", "Invalid Request");
    set_error_message(Errors::MethodNotFound.code, "en-US", "Method not found");
    set_error_message(Errors::InvalidParams.code, "en-US", "Invalid params");
    set_error_message(Errors::InternalError.code, "en-US", "Internal error");

    // Добавление других языков по умолчанию
    set_error_message(Errors::ParseError.code, "ru-RU", "Ошибка разбора");
    set_error_message(Errors::InvalidRequest.code, "ru-RU", "Неверный запрос");
    set_error_message(Errors::MethodNotFound.code, "ru-RU", "Метод не найден");
    set_error_message(Errors::InvalidParams.code, "ru-RU",
                      "Неверные параметры");
    set_error_message(Errors::InternalError.code, "ru-RU", "Внутренняя ошибка");
  }

  void register_method(const MethodInfo &method) {
    std::lock_guard lock(mutex_);
    methods_[method.name] = method;
  }

  bool has_method(const std::string &name) const {
    std::lock_guard lock(mutex_);
    return methods_.find(name) != methods_.end();
  }

  MethodInfo get_method(const std::string &name) const {
    std::lock_guard lock(mutex_);
    auto it = methods_.find(name);
    if (it != methods_.end()) {
      return it->second;
    }
    throw std::runtime_error("Method not found: " + name);
  }

  void set_error_message(int code, const std::string &language,
                         const std::string &message) {
    std::string key = std::to_string(code) + "_" + language;
    error_messages_[key] = message;
  }

  std::string get_error_message(int code, const std::string &language) const {
    std::string key = std::to_string(code) + "_" + language;
    auto it = error_messages_.find(key);
    if (it != error_messages_.end()) {
      return it->second;
    }

    // Fallback to default language
    key = std::to_string(code) + "_" + default_language_;
    it = error_messages_.find(key);
    if (it != error_messages_.end()) {
      return it->second;
    }

    return "Unknown error";
  }

  std::vector<std::string> get_method_names() const {
    std::vector<std::string> names;
    for (const auto &pair : methods_) {
      names.push_back(pair.first);
    }
    return names;
  }
};

// Обработчик сессии WebSocket
class Session : public std::enable_shared_from_this<Session> {
public:
  Session(tcp::socket &&socket, MethodRegistry &registry,
          const std::string &version, Logging::Logger &logger)
      : ws_(std::move(socket)), registry_(registry), version_(version),
        logger_(logger) {}

  void run() {
    ws_.set_option(
        websocket::stream_base::timeout::suggested(beast::role_type::server));

    ws_.set_option(
        websocket::stream_base::decorator([](websocket::response_type &res) {
          res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) +
                                           " JSON-RPC 2.0 WebSocket Server");
        }));

    ws_.async_accept(
        beast::bind_front_handler(&Session::on_accept, shared_from_this()));
  }

private:
  void on_accept(beast::error_code ec) {
    if (ec) {
      LOG_ERROR(logger_.get(), "Accept error: {}", ec.message());
      return;
    }
    do_read();
  }

  void do_read() {
    ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read,
                                                      shared_from_this()));
  }

  void safe_write(const json &response) {
    std::string response_str = response.dump();

    // Очищаем предыдущий буфер
    write_buffer_.consume(write_buffer_.size());

    // Записываем в буфер
    auto buffers{write_buffer_.prepare(response_str.size())};
    std::memcpy(buffers.data(), response_str.data(), response_str.size());
    write_buffer_.commit(response_str.size());

    ws_.text(true);
    ws_.async_write(
        write_buffer_.data(),
        beast::bind_front_handler(&Session::on_write, shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec == websocket::error::closed) {
      return;
    }
    if (ec) {
      LOG_ERROR(logger_.get(), "Read error: {}", ec.message());
      return;
    }

    try {
      std::string request_str = beast::buffers_to_string(buffer_.data());
      buffer_.consume(buffer_.size());

      auto request_json = json::parse(request_str);

      json response;
      if (request_json.is_array()) {
        response = process_batch_request(request_json);
      } else {
        response = process_request(request_json);
      }

      safe_write(response);

    } catch (const json::parse_error &e) {
      auto response = create_error_response(json(), -32700, "Parse error");
      safe_write(response);
    } catch (const std::exception &e) {
      auto response = create_error_response(json(), -32603, e.what());
      safe_write(response);
    }
  }

  json process_request(const json &request) {
    // Проверка структуры запроса
    if (!request.is_object() || !request.contains("jsonrpc") ||
        request["jsonrpc"] != "2.0" || !request.contains("method") ||
        !request["method"].is_string()) {
      return create_error_response(nullptr, Errors::InvalidRequest, "en-US");
    }

    // Получаем ID запроса если есть
    json id = request.contains("id") ? request["id"] : json();

    try {
      std::string method_name = request["method"];
      json params = request.contains("params") ? request["params"] : json();
      std::string language = "en-US";

      // Извлекаем язык из параметров если есть
      if (params.is_object() && params.contains("language")) {
        language = params["language"];
      }

      if (!registry_.has_method(method_name)) {
        return create_error_response(id, Errors::MethodNotFound, language);
      }

      MethodInfo method = registry_.get_method(method_name);

      // Выполняем метод
      json result = method.handler(params, language);

      // Формируем успешный ответ
      json response = {{"jsonrpc", "2.0"}, {"result", result}, {"id", id}};

      return response;
    } catch (const JsonRpcError &e) {
      return create_error_response(id, e, "en-US");
    } catch (const std::exception &e) {
      return create_error_response(
          id, JsonRpcError(Errors::InternalError.code, e.what()), "en-US");
    }
  }

  json process_batch_request(const json &requests) {
    json responses = json::array();

    for (const auto &request : requests) {
      json response = process_request(request);
      if (!response.is_null()) {
        responses.push_back(response);
      }
    }

    return responses;
  }

  json create_error_response(const json &id, const JsonRpcError &error,
                             const std::string &language) {
    std::string localized_message =
        registry_.get_error_message(error.code, language);

    json response = {
        {"jsonrpc", "2.0"},
        {"error", {{"code", error.code}, {"message", localized_message}}},
        {"id", id.is_null() ? json() : id}};

    if (!error.data.is_null()) {
      response["error"]["data"] = error.data;
    }

    return response;
  }

  json create_error_response(const json &id, int code,
                             const std::string &message,
                             const json &data = json()) {
    std::string localized_message = registry_.get_error_message(code, "en-US");

    json response = {
        {"jsonrpc", "2.0"},
        {"error", {{"code", code}, {"message", localized_message}}},
        {"id", id.is_null() ? json() : id}};

    if (!data.is_null()) {
      response["error"]["data"] = data;
    }

    return response;
  }

  // Перегрузка для const char*
  json create_error_response(const json &id, int code, const char *message,
                             const json &data = json()) {
    return create_error_response(id, code, std::string(message), data);
  }

  void send_error_response(const json &id, const JsonRpcError &error,
                           const std::string &language) {
    json response = create_error_response(id, error, language);

    ws_.text(true);
    ws_.async_write(
        net::buffer(response.dump()),
        beast::bind_front_handler(&Session::on_write, shared_from_this()));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
      LOG_ERROR(logger_.get(), "Write error: {}", ec.message());
      return;
    }

    // Чистим буфер и ждем следующего сообщения
    buffer_.consume(buffer_.size());
    do_read();
  }

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
         const std::string &version, Logging::Logger &logger)
      : ioc_(ioc), acceptor_(ioc), version_(version), logger_(logger) {

    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      throw std::runtime_error("Failed to open acceptor: " + ec.message());
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
      throw std::runtime_error("Failed to set socket option: " + ec.message());
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
      throw std::runtime_error("Failed to bind: " + ec.message());
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
      throw std::runtime_error("Failed to listen: " + ec.message());
    }
  }

  MethodRegistry &get_registry() { return registry_; }

  void run() { do_accept(); }

private:
  void do_accept() {
    acceptor_.async_accept(net::make_strand(ioc_),
                           beast::bind_front_handler(&Server::on_accept, this));
  }

  void on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
      LOG_ERROR(logger_.get(), "Accept error: {}", ec.message());
    } else {
      std::make_shared<Session>(std::move(socket), registry_, version_, logger_)
          ->run();
    }

    do_accept();
  }

private:
  net::io_context &ioc_;
  tcp::acceptor acceptor_;
  MethodRegistry registry_;
  std::string version_;
  Logging::Logger &logger_;
};

} // namespace JsonRpc
