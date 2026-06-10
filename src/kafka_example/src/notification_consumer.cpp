#include "kafka_config.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <kafka/KafkaConsumer.h>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::atomic<bool> running{true};

void signalHandler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    std::cout << "\n[Notification Consumer] Shutting down..." << std::endl;
    running = false;
  }
}

class NotificationConsumer {
public:
  NotificationConsumer(const std::string &brokers, const std::string &group_id)
      : consumer_(createConsumer(brokers, group_id)) {}

  void start() {
    consumer_.subscribe({KafkaConfig::CUSTOMER_NOTIFICATION_TOPIC});

    std::cout
        << "[Notification Consumer] Listening for customer notifications..."
        << std::endl;
    std::cout << "[Notification Consumer] Press Ctrl+C to exit" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    while (running) {
      auto records = consumer_.poll(std::chrono::milliseconds(100));

      for (const auto &record : records) {
        if (!record.error()) {
          processNotification(record);
        } else if (record.error().value() != RD_KAFKA_RESP_ERR__TIMED_OUT) {
          std::cerr << "[Notification Consumer] Error: "
                    << record.error().message() << std::endl;
        }
      }
    }

    consumer_.close();
  }

private:
  kafka::clients::consumer::KafkaConsumer consumer_;
  std::map<std::string, std::vector<std::string>> customer_notifications_;

  kafka::clients::consumer::KafkaConsumer
  createConsumer(const std::string &brokers, const std::string &group_id) {
    kafka::Properties props;
    props.put("bootstrap.servers", brokers);
    props.put("group.id", group_id);
    props.put("auto.offset.reset", "earliest");
    props.put("enable.auto.commit", "true");

    return kafka::clients::consumer::KafkaConsumer(props);
  }

  void processNotification(
      const kafka::clients::consumer::ConsumerRecord &record) {
    try {
      const auto payload_buffer = record.value();
      std::string_view payload(
          static_cast<const char *>(payload_buffer.data()),
          payload_buffer.size());
      json notification = json::parse(payload);

      std::string customer_id = notification["customer_id"].get<std::string>();
      std::string order_id = notification["order_id"].get<std::string>();
      std::string message = notification["message"].get<std::string>();
      int type = notification["type"].get<int>();

      std::cout << "\n[Notification] To: Customer " << customer_id
                << std::endl;
      std::cout << "   Order: " << order_id << std::endl;
      std::cout << "   Type: " << getNotificationType(type) << std::endl;
      std::cout << "   Message: " << message << std::endl;

      sendEmail(customer_id, message);
      customer_notifications_[customer_id].push_back(message);

      std::cout << "   Notification sent successfully" << std::endl;
      std::cout << std::string(60, '-') << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[Notification Consumer] Failed to process: " << e.what()
                << std::endl;
    }
  }

  std::string getNotificationType(int type) {
    switch (type) {
    case 0:
      return "ORDER_CONFIRMED";
    case 1:
      return "STATUS_UPDATED";
    case 2:
      return "ORDER_CANCELLED";
    default:
      return "UNKNOWN";
    }
  }

  void sendEmail(const std::string &customer_id, const std::string &message) {
    std::cout << "   [Email Service] Sending to customer_" << customer_id
              << "@example.com" << std::endl;
    std::cout << "   [Email Content] " << message << std::endl;
  }
};

int main() {
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  std::cout << "=== Customer Notification Service ===" << std::endl;
  std::cout << "Handling customer notifications in real-time" << std::endl;

  NotificationConsumer consumer(KafkaConfig::BROKERS,
                                KafkaConfig::NOTIFICATION_CONSUMER_GROUP);
  consumer.start();

  std::cout << "[Notification Consumer] Stopped." << std::endl;
  return 0;
}
