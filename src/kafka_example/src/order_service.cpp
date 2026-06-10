#include "order_service.hpp"

#include <chrono>
#include <iostream>
#include <mutex>

OrderService::OrderService(const std::string &brokers) {
  kafka::Properties props;
  props.put("bootstrap.servers", brokers);
  props.put("enable.idempotence", "true");
  props.put("acks", "all");
  props.put("client.id", "order-service-producer");
  props.put("message.send.max.retries", "2147483647");

  producer_ =
      std::make_unique<kafka::clients::producer::KafkaProducer>(props);

  std::cout << "[OrderService] Initialized with modern-cpp-kafka" << std::endl;
}

template <typename T>
bool OrderService::sendIdempotent(const std::string &topic,
                                  const std::string &key, const T &event,
                                  const std::string &transaction_id) {
  if (idempotency_cache_.contains(transaction_id)) {
    std::cout << "[Idempotency] Duplicate transaction rejected: "
              << transaction_id << std::endl;
    return true;
  }

  try {
    json value = event.toJson();
    value["transaction_id"] = transaction_id;
    std::string value_str = value.dump();

    kafka::clients::producer::ProducerRecord record(
        topic, kafka::Key(key.data(), key.size()),
        kafka::Value(value_str.data(), value_str.size()));

    producer_->syncSend(record);

    std::cout << "[Producer] Message sent to " << topic << std::endl;

    idempotency_cache_.add(transaction_id);
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[Producer] Failed: " << e.what() << std::endl;
    return false;
  }
}

bool OrderService::confirmOrder(const OrderConfirmationEvent &event) {
  std::cout << "[OrderService] Confirming order: " << event.order_id
            << std::endl;

  Order order;
  order.order_id = event.order_id;
  order.customer_id = event.customer_id;
  order.products = event.products;
  order.status = OrderStatus::NEW;
  order.created_at = std::chrono::system_clock::now();
  order.updated_at = order.created_at;

  {
    std::unique_lock lock(orders_mutex_);
    orders_[event.order_id] = order;
  }

  const bool success = sendIdempotent(ORDER_CONFIRMATION_TOPIC, event.order_id,
                                      event, event.transaction_id);

  if (success) {
    sendNotification(event.customer_id, event.order_id,
                     NotificationType::ORDER_CONFIRMED,
                     "Your order has been confirmed!");

    json details = {{"order_id", event.order_id},
                    {"customer_id", event.customer_id}};
    auditOperation("ORDER_CONFIRMED", details);
  }

  return success;
}

bool OrderService::updateOrderStatus(const OrderStatusUpdateEvent &event) {
  std::cout << "[OrderService] Updating order: " << event.order_id << " -> "
            << statusToString(event.status) << std::endl;

  {
    std::shared_lock lock(orders_mutex_);
    if (orders_.find(event.order_id) == orders_.end()) {
      std::cerr << "[OrderService] Order not found: " << event.order_id
                << std::endl;
      return false;
    }
  }

  {
    std::unique_lock lock(orders_mutex_);
    orders_[event.order_id].status = event.status;
    orders_[event.order_id].updated_at = std::chrono::system_clock::now();
  }

  const bool success = sendIdempotent(ORDER_STATUS_UPDATE_TOPIC, event.order_id,
                                      event, event.transaction_id);

  if (success) {
    std::string customer_id;
    {
      std::shared_lock lock(orders_mutex_);
      customer_id = orders_[event.order_id].customer_id;
    }

    std::string message = "Your order status has been updated to: " +
                          statusToString(event.status);
    sendNotification(customer_id, event.order_id,
                     NotificationType::STATUS_UPDATED, message);

    json details = {{"order_id", event.order_id},
                    {"new_status", statusToString(event.status)}};
    auditOperation("ORDER_STATUS_UPDATED", details);
  }

  return success;
}

bool OrderService::cancelOrder(const OrderCancellationEvent &event) {
  std::cout << "[OrderService] Cancelling order: " << event.order_id
            << " - Reason: " << event.reason << std::endl;

  {
    std::shared_lock lock(orders_mutex_);
    if (orders_.find(event.order_id) == orders_.end()) {
      std::cerr << "[OrderService] Order not found: " << event.order_id
                << std::endl;
      return false;
    }
  }

  {
    std::unique_lock lock(orders_mutex_);
    orders_[event.order_id].status = OrderStatus::CANCELLED;
    orders_[event.order_id].updated_at = std::chrono::system_clock::now();
  }

  const bool success = sendIdempotent(ORDER_CANCELLATION_TOPIC, event.order_id,
                                      event, event.transaction_id);

  if (success) {
    std::string customer_id;
    {
      std::shared_lock lock(orders_mutex_);
      customer_id = orders_[event.order_id].customer_id;
    }

    sendNotification(customer_id, event.order_id,
                     NotificationType::ORDER_CANCELLED,
                     "Your order has been cancelled. Reason: " + event.reason);

    json details = {{"order_id", event.order_id}, {"reason", event.reason}};
    auditOperation("ORDER_CANCELLED", details);
  }

  return success;
}

std::optional<Order> OrderService::getOrder(const std::string &order_id) {
  std::shared_lock lock(orders_mutex_);
  auto it = orders_.find(order_id);
  if (it != orders_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void OrderService::sendNotification(const std::string &customer_id,
                                    const std::string &order_id,
                                    NotificationType type,
                                    const std::string &message) {
  CustomerNotificationEvent event;
  event.customer_id = customer_id;
  event.order_id = order_id;
  event.type = type;
  event.message = message;
  event.timestamp = std::chrono::system_clock::now();

  try {
    std::string value_str = event.toJson().dump();
    kafka::clients::producer::ProducerRecord record(
        CUSTOMER_NOTIFICATION_TOPIC,
        kafka::Key(customer_id.data(), customer_id.size()),
        kafka::Value(value_str.data(), value_str.size()));
    producer_->syncSend(record);
    std::cout << "[Notification] Sent to customer " << customer_id << ": "
              << message << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[Notification] Failed: " << e.what() << std::endl;
  }
}

void OrderService::auditOperation(const std::string &operation,
                                  const json &details) {
  AuditEvent event;
  event.operation = operation;
  event.details = details;
  event.service = "order-service";
  event.timestamp = std::chrono::system_clock::now();

  try {
    std::string value_str = event.toJson().dump();
    kafka::clients::producer::ProducerRecord record(
        AUDIT_TOPIC, kafka::Key(operation.data(), operation.size()),
        kafka::Value(value_str.data(), value_str.size()));
    producer_->syncSend(record);
    std::cout << "[Audit] Logged: " << operation << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[Audit] Failed: " << e.what() << std::endl;
  }
}

std::string OrderService::statusToString(OrderStatus status) {
  switch (status) {
  case OrderStatus::NEW:
    return "new";
  case OrderStatus::PROCESSING:
    return "processing";
  case OrderStatus::SHIPPED:
    return "shipped";
  case OrderStatus::CANCELLED:
    return "cancelled";
  default:
    return "unknown";
  }
}

template bool OrderService::sendIdempotent<OrderConfirmationEvent>(
    const std::string &, const std::string &, const OrderConfirmationEvent &,
    const std::string &);
template bool OrderService::sendIdempotent<OrderStatusUpdateEvent>(
    const std::string &, const std::string &, const OrderStatusUpdateEvent &,
    const std::string &);
template bool OrderService::sendIdempotent<OrderCancellationEvent>(
    const std::string &, const std::string &, const OrderCancellationEvent &,
    const std::string &);
