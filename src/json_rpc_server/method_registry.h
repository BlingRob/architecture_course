#pragma once

#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace JsonRpc {

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

// Типы для обработчиков
using MethodHandler = std::function<json(const json &, const std::string &)>;

// Структура для описания метода
struct MethodInfo {
  std::string name;
  std::string description;
  json params_schema;
  json result_schema;
  MethodHandler handler;
  std::vector<int> allowed_errors;
};

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

} // namespace JsonRpc
