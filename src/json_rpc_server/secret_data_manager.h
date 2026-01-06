#pragma once

#include "json_rpc_server.h"
#include <mutex>
#include <string>
#include <unordered_map>

namespace SecretData {

// Уровни доступа
enum class AccessLevel { USER, ADMIN, SUPER_ADMIN };

// Структура для хранения секретных данных
struct SecretData {
  std::string id;
  std::string data;
  AccessLevel min_access_level;
  std::string owner;
  time_t created_at;
  time_t updated_at;
};

// Менеджер секретных данных
class SecretDataManager {
public:
  SecretDataManager();

  // Регистрация всех методов API
  void RegisterMethods(JsonRpc::MethodRegistry &registry);

private:
  void registerManageSecretDataMethod(JsonRpc::MethodRegistry &registry);

  void registerListDataMethod(JsonRpc::MethodRegistry &registry);

  void registerGetStatsMethod(JsonRpc::MethodRegistry &registry);

  void registerErrorMessages(JsonRpc::MethodRegistry &registry);

  AccessLevel stringToAccessLevel(const std::string &level_str);

  std::string accessLevelToString(AccessLevel level);

  bool hasPermission(AccessLevel user_level, AccessLevel required_level);

  json handleManageSecretData(const json &params, const std::string &language);

  json handleAddData(const std::string &data_id, const std::string &user_data,
                     AccessLevel user_access_level,
                     const std::string &language);

  json handleGetData(const std::string &data_id, AccessLevel user_access_level,
                     const std::string &language);

  json handleDeleteData(const std::string &data_id,
                        AccessLevel user_access_level,
                        const std::string &language);

  json handleListSecretData(const json &params, const std::string &language);

  json handleGetStats(const json &params, const std::string &language);

private:
  std::unordered_map<std::string, SecretData> data_store_;
  std::mutex mutex_;

  // Коды ошибок приложения
  enum AppErrorCodes {
    ACCESS_DENIED = 100,
    INVALID_ACCESS_LEVEL = 101,
    DATA_NOT_FOUND = 102,
    INVALID_ACTION = 103,
    DATA_ALREADY_EXISTS = 104,
    MISSING_REQUIRED_FIELD = 105
  };
};

} // namespace SecretData
