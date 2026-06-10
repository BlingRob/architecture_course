#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

enum class OrderStatus { NEW, PROCESSING, SHIPPED, CANCELLED };

enum class NotificationType {
  ORDER_CONFIRMED,
  STATUS_UPDATED,
  ORDER_CANCELLED
};

struct Product {
  std::string id;
  std::string name;
  double price;
  json toJson() const;
  static Product fromJson(const json &j);
};

struct Order {
  std::string order_id;
  std::string customer_id;
  std::vector<Product> products;
  OrderStatus status;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point updated_at;
  json toJson() const;
  static Order fromJson(const json &j);
};

struct OrderConfirmationEvent {
  std::string order_id;
  std::string customer_id;
  std::vector<Product> products;
  OrderStatus status;
  std::string transaction_id;
  std::chrono::system_clock::time_point timestamp;
  json toJson() const;
  static OrderConfirmationEvent fromJson(const json &j);
};

struct OrderStatusUpdateEvent {
  std::string order_id;
  OrderStatus status;
  std::string transaction_id;
  std::chrono::system_clock::time_point timestamp;
  json toJson() const;
  static OrderStatusUpdateEvent fromJson(const json &j);
};

struct OrderCancellationEvent {
  std::string order_id;
  std::string reason;
  std::string transaction_id;
  std::chrono::system_clock::time_point timestamp;
  json toJson() const;
  static OrderCancellationEvent fromJson(const json &j);
};

struct CustomerNotificationEvent {
  std::string customer_id;
  std::string order_id;
  NotificationType type;
  std::string message;
  std::chrono::system_clock::time_point timestamp;
  json toJson() const;
  static CustomerNotificationEvent fromJson(const json &j);
};

struct AuditEvent {
  std::string operation;
  json details;
  std::string service;
  std::chrono::system_clock::time_point timestamp;
  json toJson() const;
  static AuditEvent fromJson(const json &j);
};
