#pragma once

#include "idempotency_cache.hpp"
#include "models.hpp"

#include <kafka/KafkaProducer.h>

#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

class OrderService {
public:
  explicit OrderService(const std::string &brokers);

  bool confirmOrder(const OrderConfirmationEvent &event);
  bool updateOrderStatus(const OrderStatusUpdateEvent &event);
  bool cancelOrder(const OrderCancellationEvent &event);

  std::optional<Order> getOrder(const std::string &order_id);

private:
  std::unique_ptr<kafka::clients::producer::KafkaProducer> producer_;

  std::unordered_map<std::string, Order> orders_;
  mutable std::shared_mutex orders_mutex_;

  IdempotencyCache idempotency_cache_;

  const std::string ORDER_CONFIRMATION_TOPIC = "order-confirmation";
  const std::string ORDER_STATUS_UPDATE_TOPIC = "order-status-update";
  const std::string ORDER_CANCELLATION_TOPIC = "order-cancellation";
  const std::string CUSTOMER_NOTIFICATION_TOPIC = "customer-notification";
  const std::string AUDIT_TOPIC = "audit-log";

  void sendNotification(const std::string &customer_id,
                        const std::string &order_id, NotificationType type,
                        const std::string &message);
  void auditOperation(const std::string &operation, const json &details);
  std::string statusToString(OrderStatus status);

  template <typename T>
  bool sendIdempotent(const std::string &topic, const std::string &key,
                      const T &event, const std::string &transaction_id);
};
