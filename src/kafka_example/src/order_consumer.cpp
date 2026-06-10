#include "kafka_config.hpp"

#include <atomic>
#include <csignal>
#include <iostream>
#include <kafka/KafkaConsumer.h>

std::atomic<bool> running{true};

void signalHandler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    running = false;
  }
}

int main() {
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  std::cout << "=== Order Consumer Service ===" << std::endl;
  std::cout << "Processing orders from Kafka..." << std::endl;

  kafka::Properties props;
  props.put("bootstrap.servers", KafkaConfig::BROKERS);
  props.put("group.id", KafkaConfig::ORDER_CONSUMER_GROUP);
  props.put("auto.offset.reset", "earliest");
  props.put("enable.auto.commit", "true");
  props.put("auto.commit.interval.ms", "5000");

  kafka::clients::consumer::KafkaConsumer consumer(props);
  consumer.subscribe({KafkaConfig::ORDER_CONFIRMATION_TOPIC,
                      KafkaConfig::ORDER_STATUS_UPDATE_TOPIC,
                      KafkaConfig::ORDER_CANCELLATION_TOPIC});

  while (running) {
    auto records = consumer.poll(std::chrono::milliseconds(100));

    for (const auto &record : records) {
      if (!record.error()) {
        std::cout << "\n[Consumer] Processing record:" << std::endl;
        std::cout << "  Topic: " << record.topic() << std::endl;
        std::cout << "  Partition: " << record.partition() << std::endl;
        std::cout << "  Offset: " << record.offset() << std::endl;
        std::cout << "  Key: " << record.key().toString() << std::endl;
        std::cout << "  Value: " << record.value().toString() << std::endl;
      }
    }
  }

  consumer.close();
  std::cout << "Consumer stopped." << std::endl;
  return 0;
}
