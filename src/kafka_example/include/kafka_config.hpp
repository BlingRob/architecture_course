#pragma once
#include <string>

namespace KafkaConfig {
const std::string BROKERS = "localhost:9092";

// Топики
const std::string ORDER_CONFIRMATION_TOPIC = "order-confirmation";
const std::string ORDER_STATUS_UPDATE_TOPIC = "order-status-update";
const std::string ORDER_CANCELLATION_TOPIC = "order-cancellation";
const std::string CUSTOMER_NOTIFICATION_TOPIC = "customer-notification";
const std::string AUDIT_TOPIC = "audit-log";

// Dead Letter Queues
const std::string ORDER_DLQ_TOPIC = "order-confirmation-dlq";
const std::string STATUS_DLQ_TOPIC = "order-status-dlq";

// Consumer Groups
const std::string ORDER_CONSUMER_GROUP = "order-processing-group";
const std::string NOTIFICATION_CONSUMER_GROUP = "notification-group";
const std::string AUDIT_CONSUMER_GROUP = "audit-group";

// Настройки producer
const std::string PRODUCER_CONFIG = R"(
        enable.idempotence=true
        acks=all
        retries=2147483647
        max.in.flight.requests.per.connection=5
    )";

// Настройки consumer
const std::string CONSUMER_CONFIG = R"(
        auto.offset.reset=earliest
        enable.auto.commit=false
    )";
} // namespace KafkaConfig