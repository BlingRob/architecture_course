#include "kafka_config.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <kafka/KafkaConsumer.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

std::atomic<bool> running{true};

void signalHandler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    std::cout << "\n[Audit Consumer] Shutting down..." << std::endl;
    running = false;
  }
}

class AuditConsumer {
public:
  AuditConsumer(const std::string &brokers, const std::string &group_id)
      : consumer_(createConsumer(brokers, group_id)) {
    audit_file_.open("audit_log.txt", std::ios::app);
    if (!audit_file_.is_open()) {
      std::cerr << "[Audit Consumer] Failed to open audit file!" << std::endl;
    }
  }

  void start() {
    consumer_.subscribe({KafkaConfig::AUDIT_TOPIC});

    std::cout << "[Audit Consumer] Listening on topic: "
              << KafkaConfig::AUDIT_TOPIC << std::endl;
    std::cout << "[Audit Consumer] Writing audit logs to audit_log.txt"
              << std::endl;
    std::cout << "[Audit Consumer] Press Ctrl+C to exit" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    while (running) {
      auto records = consumer_.poll(std::chrono::milliseconds(100));

      for (const auto &record : records) {
        if (!record.error()) {
          processRecord(record);
        } else if (record.error().value() != RD_KAFKA_RESP_ERR__TIMED_OUT) {
          std::cerr << "[Audit Consumer] Error: " << record.error().message()
                    << std::endl;
        }
      }
    }

    consumer_.close();
    if (audit_file_.is_open()) {
      audit_file_.close();
    }
  }

private:
  kafka::clients::consumer::KafkaConsumer consumer_;
  std::ofstream audit_file_;

  kafka::clients::consumer::KafkaConsumer
  createConsumer(const std::string &brokers, const std::string &group_id) {
    kafka::Properties props;
    props.put("bootstrap.servers", brokers);
    props.put("group.id", group_id);
    props.put("auto.offset.reset", "earliest");
    props.put("enable.auto.commit", "true");
    props.put("auto.commit.interval.ms", "1000");

    return kafka::clients::consumer::KafkaConsumer(props);
  }

  void processRecord(const kafka::clients::consumer::ConsumerRecord &record) {
    try {
      const auto payload_buffer = record.value();
      std::string_view payload(
          static_cast<const char *>(payload_buffer.data()),
          payload_buffer.size());
      json audit_entry = json::parse(payload);

      std::string timestamp =
          formatTimestamp(audit_entry["timestamp"].get<long long>());
      std::string operation = audit_entry["operation"].get<std::string>();

      std::cout << "\n[Audit Entry]" << std::endl;
      std::cout << "  Time: " << timestamp << std::endl;
      std::cout << "  Operation: " << operation << std::endl;
      std::cout << "  Details: " << audit_entry["details"].dump(2) << std::endl;
      std::cout << std::string(60, '-') << std::endl;

      if (audit_file_.is_open()) {
        audit_file_ << "[" << timestamp << "] " << operation << " | "
                    << audit_entry["details"].dump() << std::endl;
        audit_file_.flush();
      }

      appendToJsonLog(audit_entry);
    } catch (const std::exception &e) {
      std::cerr << "[Audit Consumer] Failed to process audit record: "
                << e.what() << std::endl;
    }
  }

  std::string formatTimestamp(long long timestamp_ms) {
    auto time_t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms)));

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << (timestamp_ms % 1000);

    return ss.str();
  }

  void appendToJsonLog(const json &audit_entry) {
    static std::mutex json_mutex;
    std::lock_guard<std::mutex> lock(json_mutex);

    std::ifstream in_file("audit_log.json");
    json full_log;

    if (in_file.is_open()) {
      try {
        in_file >> full_log;
      } catch (...) {
        full_log = json::array();
      }
      in_file.close();
    } else {
      full_log = json::array();
    }

    full_log.push_back(audit_entry);

    std::ofstream out_file("audit_log.json");
    if (out_file.is_open()) {
      out_file << full_log.dump(2);
      out_file.close();
    }
  }
};

int main() {
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  std::cout << "=== Audit Consumer Service ===" << std::endl;
  std::cout << "Real-time audit logging for order management system"
            << std::endl;

  AuditConsumer consumer(KafkaConfig::BROKERS,
                         KafkaConfig::AUDIT_CONSUMER_GROUP);
  consumer.start();

  std::cout << "[Audit Consumer] Stopped." << std::endl;
  return 0;
}
