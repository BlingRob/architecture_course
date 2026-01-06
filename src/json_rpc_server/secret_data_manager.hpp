#pragma once

#include "json_rpc_server.hpp"
#include <unordered_map>
#include <mutex>
#include <string>

namespace SecretData {

// Уровни доступа
enum class AccessLevel {
    USER,
    ADMIN,
    SUPER_ADMIN
};

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
    
public:
    SecretDataManager() {
        // Инициализация можно добавить здесь
    }
    
    // Регистрация всех методов API
    void register_methods(JsonRpc::MethodRegistry& registry) {
        register_manage_secret_data_method(registry);
        register_list_data_method(registry);
        register_get_stats_method(registry);
        
        // Регистрация сообщений об ошибках для разных языков
        register_error_messages(registry);
    }
    
private:
    void register_manage_secret_data_method(JsonRpc::MethodRegistry& registry) {
        JsonRpc::MethodInfo method;
        method.name = "manageSecretData";
        method.description = "Manage secure data storage with add, get, and delete operations";
        
        // Схема параметров
        method.params_schema = {
            {"type", "object"},
            {"properties", {
                {"action", {
                    {"type", "string"},
                    {"enum", {"add", "get", "delete"}}
                }},
                {"dataID", {
                    {"type", "string"}
                }},
                {"userData", {
                    {"type", "string"}
                }},
                {"accessLevel", {
                    {"type", "string"},
                    {"enum", {"user", "admin", "superAdmin"}}
                }},
                {"language", {
                    {"type", "string"},
                    {"enum", {"en-US", "ru-RU", "fr-FR", "es-ES"}},
                    {"default", "en-US"}
                }}
            }},
            {"required", {"action", "dataID", "accessLevel"}}
        };
        
        // Схема результата
        method.result_schema = {
            {"type", "object"},
            {"properties", {
                {"status", {
                    {"type", "string"},
                    {"enum", json::array({"success", "failure"})}
                }},
                {"message", {
                    {"type", "string"}
                }},
                {"data", {
                    {"type", json::array({"string", "null"})}
                }}
            }}
        };
        
        // Обработчик метода
        method.handler = [this](const json& params, const std::string& language) {
            return handle_manage_secret_data(params, language);
        };
        
        // Допустимые ошибки
        method.allowed_errors = {
            -32600, -32601, -32602, -32603,  // Стандартные JSON-RPC ошибки
            100, 101, 102, 103, 104, 105     // Ошибки приложения
        };
        
        registry.register_method(method);
    }
    
    void register_list_data_method(JsonRpc::MethodRegistry& registry) {
        JsonRpc::MethodInfo method;
        method.name = "listSecretData";
        method.description = "List all secret data IDs accessible by the user";
        
        method.params_schema = {
            {"type", "object"},
            {"properties", {
                {"accessLevel", {
                    {"type", "string"},
                    {"enum", {"user", "admin", "superAdmin"}}
                }},
                {"language", {
                    {"type", "string"},
                    {"enum", {"en-US", "ru-RU", "fr-FR", "es-ES"}},
                    {"default", "en-US"}
                }}
            }},
            {"required", {"accessLevel"}}
        };
        
        method.handler = [this](const json& params, const std::string& language) {
            return handle_list_secret_data(params, language);
        };
        
        registry.register_method(method);
    }
    
    void register_get_stats_method(JsonRpc::MethodRegistry& registry) {
        JsonRpc::MethodInfo method;
        method.name = "getSecretDataStats";
        method.description = "Get statistics about secret data";
        
        method.params_schema = {
            {"type", "object"},
            {"properties", {
                {"accessLevel", {
                    {"type", "string"},
                    {"enum", {"admin", "superAdmin"}}
                }},
                {"language", {
                    {"type", "string"},
                    {"enum", {"en-US", "ru-RU", "fr-FR", "es-ES"}},
                    {"default", "en-US"}
                }}
            }},
            {"required", {"accessLevel"}}
        };
        
        method.handler = [this](const json& params, const std::string& language) {
            return handle_get_stats(params, language);
        };
        
        registry.register_method(method);
    }
    
    void register_error_messages(JsonRpc::MethodRegistry& registry) {
        // Английский
        registry.set_error_message(ACCESS_DENIED, "en-US", "Access denied");
        registry.set_error_message(INVALID_ACCESS_LEVEL, "en-US", "Invalid access level");
        registry.set_error_message(DATA_NOT_FOUND, "en-US", "Data not found");
        registry.set_error_message(INVALID_ACTION, "en-US", "Invalid action");
        registry.set_error_message(DATA_ALREADY_EXISTS, "en-US", "Data already exists");
        registry.set_error_message(MISSING_REQUIRED_FIELD, "en-US", "Missing required field");
        
        // Русский
        registry.set_error_message(ACCESS_DENIED, "ru-RU", "Доступ запрещен");
        registry.set_error_message(INVALID_ACCESS_LEVEL, "ru-RU", "Неверный уровень доступа");
        registry.set_error_message(DATA_NOT_FOUND, "ru-RU", "Данные не найдены");
        registry.set_error_message(INVALID_ACTION, "ru-RU", "Неверное действие");
        registry.set_error_message(DATA_ALREADY_EXISTS, "ru-RU", "Данные уже существуют");
        registry.set_error_message(MISSING_REQUIRED_FIELD, "ru-RU", "Отсутствует обязательное поле");
        
        // Французский
        registry.set_error_message(ACCESS_DENIED, "fr-FR", "Accès refusé");
        registry.set_error_message(INVALID_ACCESS_LEVEL, "fr-FR", "Niveau d'accès invalide");
        registry.set_error_message(DATA_NOT_FOUND, "fr-FR", "Données non trouvées");
        registry.set_error_message(INVALID_ACTION, "fr-FR", "Action invalide");
        registry.set_error_message(DATA_ALREADY_EXISTS, "fr-FR", "Données déjà existantes");
        
        // Испанский
        registry.set_error_message(ACCESS_DENIED, "es-ES", "Acceso denegado");
        registry.set_error_message(INVALID_ACCESS_LEVEL, "es-ES", "Nivel de acceso inválido");
        registry.set_error_message(DATA_NOT_FOUND, "es-ES", "Datos no encontrados");
        registry.set_error_message(INVALID_ACTION, "es-ES", "Acción inválida");
        registry.set_error_message(DATA_ALREADY_EXISTS, "es-ES", "Datos ya existen");
    }
    
    AccessLevel string_to_access_level(const std::string& level_str) {
        if (level_str == "superAdmin") return AccessLevel::SUPER_ADMIN;
        if (level_str == "admin") return AccessLevel::ADMIN;
        return AccessLevel::USER;
    }
    
    std::string access_level_to_string(AccessLevel level) {
        switch (level) {
            case AccessLevel::SUPER_ADMIN: return "superAdmin";
            case AccessLevel::ADMIN: return "admin";
            default: return "user";
        }
    }
    
    bool has_permission(AccessLevel user_level, AccessLevel required_level) {
        // SUPER_ADMIN > ADMIN > USER
        return static_cast<int>(user_level) >= static_cast<int>(required_level);
    }
    
    json handle_manage_secret_data(const json& params, const std::string& language) {
        // Валидация параметров
        if (!params.contains("action") || !params.contains("dataID") || 
            !params.contains("accessLevel")) {
            throw JsonRpc::JsonRpcError(
                MISSING_REQUIRED_FIELD,
                "Missing required field: action, dataID, or accessLevel"
            );
        }
        
        std::string action = params["action"];
        std::string data_id = params["dataID"];
        AccessLevel user_access_level = string_to_access_level(params["accessLevel"]);
        std::string user_data = params.contains("userData") ? params["userData"] : "";
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (action == "add") {
            return handle_add_data(data_id, user_data, user_access_level, language);
        } else if (action == "get") {
            return handle_get_data(data_id, user_access_level, language);
        } else if (action == "delete") {
            return handle_delete_data(data_id, user_access_level, language);
        } else {
            throw JsonRpc::JsonRpcError(INVALID_ACTION, "Invalid action: " + action);
        }
    }
    
    json handle_add_data(const std::string& data_id, const std::string& user_data,
                        AccessLevel user_access_level, const std::string& language) {
        // Проверяем, существует ли уже данные с таким ID
        if (data_store_.find(data_id) != data_store_.end()) {
            throw JsonRpc::JsonRpcError(DATA_ALREADY_EXISTS, "Data with ID already exists");
        }
        
        // Создаем новые данные
        SecretData new_data;
        new_data.id = data_id;
        new_data.data = user_data;
        new_data.min_access_level = user_access_level; // По умолчанию требуемый уровень - уровень пользователя
        new_data.owner = "user"; // В реальной системе здесь был бы реальный пользователь
        new_data.created_at = time(nullptr);
        new_data.updated_at = time(nullptr);
        
        data_store_[data_id] = new_data;
        
        return {
            {"status", "success"},
            {"message", "Data added successfully"},
            {"data", nullptr}
        };
    }
    
    json handle_get_data(const std::string& data_id, AccessLevel user_access_level,
                        const std::string& language) {
        auto it = data_store_.find(data_id);
        if (it == data_store_.end()) {
            throw JsonRpc::JsonRpcError(DATA_NOT_FOUND, "Data not found");
        }
        
        const SecretData& data = it->second;
        
        // Проверяем доступ
        if (!has_permission(user_access_level, data.min_access_level)) {
            throw JsonRpc::JsonRpcError(ACCESS_DENIED, "Access denied to this data");
        }
        
        return {
            {"status", "success"},
            {"message", "Data retrieved successfully"},
            {"data", data.data}
        };
    }
    
    json handle_delete_data(const std::string& data_id, AccessLevel user_access_level,
                           const std::string& language) {
        // Только superAdmin может удалять данные
        if (user_access_level != AccessLevel::SUPER_ADMIN) {
            throw JsonRpc::JsonRpcError(ACCESS_DENIED, "Only superAdmin can delete data");
        }
        
        auto it = data_store_.find(data_id);
        if (it == data_store_.end()) {
            throw JsonRpc::JsonRpcError(DATA_NOT_FOUND, "Data not found");
        }
        
        data_store_.erase(it);
        
        return {
            {"status", "success"},
            {"message", "Data deleted successfully"},
            {"data", nullptr}
        };
    }
    
    json handle_list_secret_data(const json& params, const std::string& language) {
        if (!params.contains("accessLevel")) {
            throw JsonRpc::JsonRpcError(MISSING_REQUIRED_FIELD, "accessLevel is required");
        }
        
        AccessLevel user_access_level = string_to_access_level(params["accessLevel"]);
        std::vector<std::string> accessible_data;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& pair : data_store_) {
            if (has_permission(user_access_level, pair.second.min_access_level)) {
                accessible_data.push_back(pair.first);
            }
        }
        
        return {
            {"status", "success"},
            {"message", "Data list retrieved successfully"},
            {"data", accessible_data},
            {"count", accessible_data.size()}
        };
    }
    
    json handle_get_stats(const json& params, const std::string& language) {
        if (!params.contains("accessLevel")) {
            throw JsonRpc::JsonRpcError(MISSING_REQUIRED_FIELD, "accessLevel is required");
        }
        
        AccessLevel user_access_level = string_to_access_level(params["accessLevel"]);
        
        // Только admin и superAdmin могут получать статистику
        if (user_access_level == AccessLevel::USER) {
            throw JsonRpc::JsonRpcError(ACCESS_DENIED, "Only admin or superAdmin can view stats");
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        int total_data = data_store_.size();
        int user_accessible = 0;
        int admin_accessible = 0;
        int super_admin_accessible = 0;
        
        for (const auto& pair : data_store_) {
            if (has_permission(AccessLevel::USER, pair.second.min_access_level)) {
                user_accessible++;
            }
            if (has_permission(AccessLevel::ADMIN, pair.second.min_access_level)) {
                admin_accessible++;
            }
            if (has_permission(AccessLevel::SUPER_ADMIN, pair.second.min_access_level)) {
                super_admin_accessible++;
            }
        }
        
        return {
            {"status", "success"},
            {"message", "Statistics retrieved successfully"},
            {"stats", {
                {"total_data", total_data},
                {"user_accessible", user_accessible},
                {"admin_accessible", admin_accessible},
                {"super_admin_accessible", super_admin_accessible},
                {"created_at", time(nullptr)}
            }}
        };
    }
};

} // namespace SecretData
