#include "models.hpp"

namespace {

long long toUnixMillis(const std::chrono::system_clock::time_point &time_point) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             time_point.time_since_epoch())
      .count();
}

std::chrono::system_clock::time_point fromUnixMillis(long long value) {
  return std::chrono::system_clock::time_point(std::chrono::milliseconds(value));
}

json productsToJson(const std::vector<Product> &products) {
  json products_json = json::array();
  for (const auto &product : products) {
    products_json.push_back(product.toJson());
  }
  return products_json;
}

std::vector<Product> productsFromJson(const json &j) {
  std::vector<Product> products;
  for (const auto &product_json : j) {
    products.push_back(Product::fromJson(product_json));
  }
  return products;
}

} // namespace

json Product::toJson() const {
  return json{{"id", id}, {"name", name}, {"price", price}};
}

Product Product::fromJson(const json &j) {
  return Product{j["id"].get<std::string>(), j["name"].get<std::string>(),
                 j["price"].get<double>()};
}

json Order::toJson() const {
  return json{{"order_id", order_id},
              {"customer_id", customer_id},
              {"products", productsToJson(products)},
              {"status", static_cast<int>(status)},
              {"created_at", toUnixMillis(created_at)},
              {"updated_at", toUnixMillis(updated_at)}};
}

Order Order::fromJson(const json &j) {
  Order order;
  order.order_id = j["order_id"].get<std::string>();
  order.customer_id = j["customer_id"].get<std::string>();
  order.products = productsFromJson(j["products"]);
  order.status = static_cast<OrderStatus>(j["status"].get<int>());
  order.created_at = fromUnixMillis(j["created_at"].get<long long>());
  order.updated_at = fromUnixMillis(j["updated_at"].get<long long>());
  return order;
}

json OrderConfirmationEvent::toJson() const {
  return json{{"order_id", order_id},
              {"customer_id", customer_id},
              {"products", productsToJson(products)},
              {"status", static_cast<int>(status)},
              {"transaction_id", transaction_id},
              {"timestamp", toUnixMillis(timestamp)}};
}

OrderConfirmationEvent OrderConfirmationEvent::fromJson(const json &j) {
  OrderConfirmationEvent event;
  event.order_id = j["order_id"].get<std::string>();
  event.customer_id = j["customer_id"].get<std::string>();
  event.products = productsFromJson(j["products"]);
  event.status = static_cast<OrderStatus>(j["status"].get<int>());
  event.transaction_id = j["transaction_id"].get<std::string>();
  event.timestamp = fromUnixMillis(j["timestamp"].get<long long>());
  return event;
}

json OrderStatusUpdateEvent::toJson() const {
  return json{{"order_id", order_id},
              {"status", static_cast<int>(status)},
              {"transaction_id", transaction_id},
              {"timestamp", toUnixMillis(timestamp)}};
}

OrderStatusUpdateEvent OrderStatusUpdateEvent::fromJson(const json &j) {
  OrderStatusUpdateEvent event;
  event.order_id = j["order_id"].get<std::string>();
  event.status = static_cast<OrderStatus>(j["status"].get<int>());
  event.transaction_id = j["transaction_id"].get<std::string>();
  event.timestamp = fromUnixMillis(j["timestamp"].get<long long>());
  return event;
}

json OrderCancellationEvent::toJson() const {
  return json{{"order_id", order_id},
              {"reason", reason},
              {"transaction_id", transaction_id},
              {"timestamp", toUnixMillis(timestamp)}};
}

OrderCancellationEvent OrderCancellationEvent::fromJson(const json &j) {
  OrderCancellationEvent event;
  event.order_id = j["order_id"].get<std::string>();
  event.reason = j["reason"].get<std::string>();
  event.transaction_id = j["transaction_id"].get<std::string>();
  event.timestamp = fromUnixMillis(j["timestamp"].get<long long>());
  return event;
}

json CustomerNotificationEvent::toJson() const {
  return json{{"customer_id", customer_id},
              {"order_id", order_id},
              {"type", static_cast<int>(type)},
              {"message", message},
              {"timestamp", toUnixMillis(timestamp)}};
}

CustomerNotificationEvent CustomerNotificationEvent::fromJson(const json &j) {
  CustomerNotificationEvent event;
  event.customer_id = j["customer_id"].get<std::string>();
  event.order_id = j["order_id"].get<std::string>();
  event.type = static_cast<NotificationType>(j["type"].get<int>());
  event.message = j["message"].get<std::string>();
  event.timestamp = fromUnixMillis(j["timestamp"].get<long long>());
  return event;
}

json AuditEvent::toJson() const {
  return json{{"operation", operation},
              {"details", details},
              {"service", service},
              {"timestamp", toUnixMillis(timestamp)}};
}

AuditEvent AuditEvent::fromJson(const json &j) {
  AuditEvent event;
  event.operation = j["operation"].get<std::string>();
  event.details = j["details"];
  event.service = j["service"].get<std::string>();
  event.timestamp = fromUnixMillis(j["timestamp"].get<long long>());
  return event;
}
