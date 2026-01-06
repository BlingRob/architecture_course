// json_rpc_server.hpp
#pragma once

#include "json_rpc_server.h"

namespace JsonRpc {

MethodRegistry::MethodRegistry() {
  // Инициализация стандартных ошибок
  SetErrorMessage(Errors::ParseError.code, "en-US", "Parse error");
  SetErrorMessage(Errors::InvalidRequest.code, "en-US", "Invalid Request");
  SetErrorMessage(Errors::MethodNotFound.code, "en-US", "Method not found");
  SetErrorMessage(Errors::InvalidParams.code, "en-US", "Invalid params");
  SetErrorMessage(Errors::InternalError.code, "en-US", "Internal error");

  // Добавление других языков по умолчанию
  SetErrorMessage(Errors::ParseError.code, "ru-RU", "Ошибка разбора");
  SetErrorMessage(Errors::InvalidRequest.code, "ru-RU", "Неверный запрос");
  SetErrorMessage(Errors::MethodNotFound.code, "ru-RU", "Метод не найден");
  SetErrorMessage(Errors::InvalidParams.code, "ru-RU", "Неверные параметры");
  SetErrorMessage(Errors::InternalError.code, "ru-RU", "Внутренняя ошибка");
}

void MethodRegistry::RegisterMethod(const MethodInfo &method) {
  std::lock_guard lock(mutex_);
  methods_[method.name] = method;
}

bool MethodRegistry::HasMethod(const std::string &name) const {
  std::lock_guard lock(mutex_);
  return methods_.find(name) != methods_.end();
}

MethodInfo MethodRegistry::GetMethod(const std::string &name) const {
  std::lock_guard lock(mutex_);
  auto it = methods_.find(name);
  if (it != methods_.end()) {
    return it->second;
  }
  throw std::runtime_error("Method not found: " + name);
}

void MethodRegistry::SetErrorMessage(int code, const std::string &language,
                                     const std::string &message) {
  std::string key = std::to_string(code) + "_" + language;
  error_messages_[key] = message;
}

std::string MethodRegistry::GetErrorMessage(int code,
                                            const std::string &language) const {
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

std::vector<std::string> MethodRegistry::GetMethodNames() const {
  std::vector<std::string> names;
  for (const auto &pair : methods_) {
    names.push_back(pair.first);
  }
  return names;
}

Session::Session(tcp::socket &&socket, MethodRegistry &registry,
                 const std::string &version, Logging::Logger &logger)
    : ws_(std::move(socket)), registry_(registry), version_(version),
      logger_(logger) {}

void Session::Run() {
  ws_.set_option(
      websocket::stream_base::timeout::suggested(beast::role_type::server));

  ws_.set_option(
      websocket::stream_base::decorator([](websocket::response_type &res) {
        res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) +
                                         " JSON-RPC 2.0 WebSocket Server");
      }));

  ws_.async_accept(
      beast::bind_front_handler(&Session::onAccept, shared_from_this()));
}

void Session::onAccept(beast::error_code ec) {
  if (ec) {
    LOG_ERROR(logger_.get(), "Accept error: {}", ec.message());
    return;
  }
  doRead();
}

void Session::doRead() {
  ws_.async_read(
      buffer_, beast::bind_front_handler(&Session::onRead, shared_from_this()));
}

void Session::safeWrite(const json &response) {
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
      beast::bind_front_handler(&Session::onWrite, shared_from_this()));
}

void Session::onRead(beast::error_code ec, std::size_t bytes_transferred) {
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
      response = processBatchRequest(request_json);
    } else {
      response = processRequest(request_json);
    }

    safeWrite(response);

  } catch (const json::parse_error &e) {
    auto response = createErrorResponse(json(), -32700, "Parse error");
    safeWrite(response);
  } catch (const std::exception &e) {
    auto response = createErrorResponse(json(), -32603, e.what());
    safeWrite(response);
  }
}

json Session::processRequest(const json &request) {
  // Проверка структуры запроса
  if (!request.is_object() || !request.contains("jsonrpc") ||
      request["jsonrpc"] != "2.0" || !request.contains("method") ||
      !request["method"].is_string()) {
    return createErrorResponse(nullptr, Errors::InvalidRequest, "en-US");
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

    if (!registry_.HasMethod(method_name)) {
      return createErrorResponse(id, Errors::MethodNotFound, language);
    }

    MethodInfo method = registry_.GetMethod(method_name);

    // Выполняем метод
    json result = method.handler(params, language);

    // Формируем успешный ответ
    json response = {{"jsonrpc", "2.0"}, {"result", result}, {"id", id}};

    return response;
  } catch (const JsonRpcError &e) {
    return createErrorResponse(id, e, "en-US");
  } catch (const std::exception &e) {
    return createErrorResponse(
        id, JsonRpcError(Errors::InternalError.code, e.what()), "en-US");
  }
}

json Session::processBatchRequest(const json &requests) {
  json responses = json::array();

  for (const auto &request : requests) {
    json response = processRequest(request);
    if (!response.is_null()) {
      responses.push_back(response);
    }
  }

  return responses;
}

json Session::createErrorResponse(const json &id, const JsonRpcError &error,
                                  const std::string &language) {
  std::string localized_message =
      registry_.GetErrorMessage(error.code, language);

  json response = {
      {"jsonrpc", "2.0"},
      {"error", {{"code", error.code}, {"message", localized_message}}},
      {"id", id.is_null() ? json() : id}};

  if (!error.data.is_null()) {
    response["error"]["data"] = error.data;
  }

  return response;
}

json Session::createErrorResponse(const json &id, int code,
                                  const std::string &message,
                                  const json &data) {
  std::string localized_message = registry_.GetErrorMessage(code, "en-US");

  json response = {{"jsonrpc", "2.0"},
                   {"error", {{"code", code}, {"message", localized_message}}},
                   {"id", id.is_null() ? json() : id}};

  if (!data.is_null()) {
    response["error"]["data"] = data;
  }

  return response;
}

// Перегрузка для const char*
json Session::createErrorResponse(const json &id, int code, const char *message,
                                  const json &data) {
  return createErrorResponse(id, code, std::string(message), data);
}

void Session::sendErrorResponse(const json &id, const JsonRpcError &error,
                                const std::string &language) {
  json response = createErrorResponse(id, error, language);

  ws_.text(true);
  ws_.async_write(
      net::buffer(response.dump()),
      beast::bind_front_handler(&Session::onWrite, shared_from_this()));
}

void Session::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
  if (ec) {
    LOG_ERROR(logger_.get(), "Write error: {}", ec.message());
    return;
  }

  // Чистим буфер и ждем следующего сообщения
  buffer_.consume(buffer_.size());
  doRead();
}

Server::Server(net::io_context &ioc, const tcp::endpoint &endpoint,
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

MethodRegistry &Server::get_registry() { return registry_; }

void Server::Run() { do_accept(); }

void Server::do_accept() {
  acceptor_.async_accept(net::make_strand(ioc_),
                         beast::bind_front_handler(&Server::onAccept, this));
}

void Server::onAccept(beast::error_code ec, tcp::socket socket) {
  if (ec) {
    LOG_ERROR(logger_.get(), "Accept error: {}", ec.message());
  } else {
    std::make_shared<Session>(std::move(socket), registry_, version_, logger_)
        ->Run();
  }

  do_accept();
}

} // namespace JsonRpc
