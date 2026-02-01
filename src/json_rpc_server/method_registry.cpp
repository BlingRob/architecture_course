#include "method_registry.h"

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

} // namespace JsonRpc
