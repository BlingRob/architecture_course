#include "kafka_config.hpp"
#include "order_service.hpp"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>

std::atomic<bool> running{true};

void signalHandler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    std::cout << "\nShutting down producer..." << std::endl;
    running = false;
  }
}

int main() {
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  std::cout << "=== Order Producer Service ===" << std::endl;
  std::cout << "Using Kafka brokers: " << KafkaConfig::BROKERS << std::endl;

  OrderService service(KafkaConfig::BROKERS);

  int order_counter = 1;

  while (running) {
    std::cout << "\n=== Create New Order ===" << std::endl;
    std::cout << "1. Confirm new order" << std::endl;
    std::cout << "2. Update order status" << std::endl;
    std::cout << "3. Cancel order" << std::endl;
    std::cout << "4. Exit" << std::endl;
    std::cout << "Choice: ";

    int choice;
    std::cin >> choice;

    if (choice == 4)
      break;

    switch (choice) {
    case 1: {
      OrderConfirmationEvent event;
      event.status = OrderStatus::NEW;
      event.order_id = "ORD-" + std::to_string(order_counter++);
      event.customer_id = "CUST-" + std::to_string(rand() % 100);
      event.transaction_id =
          "TX-" +
          std::to_string(
              std::chrono::system_clock::now().time_since_epoch().count());
      event.timestamp = std::chrono::system_clock::now();

      Product p1{"PROD-001", "Laptop", 1500.00};
      Product p2{"PROD-002", "Mouse", 25.99};
      event.products = {p1, p2};

      if (service.confirmOrder(event)) {
        std::cout << "✓ Order " << event.order_id << " confirmed!" << std::endl;
      }
      break;
    }
    case 2: {
      std::string order_id;
      std::cout << "Enter order ID: ";
      std::cin >> order_id;

      OrderStatusUpdateEvent event;
      event.order_id = order_id;
      event.status = OrderStatus::PROCESSING;
      event.transaction_id =
          "TX-" +
          std::to_string(
              std::chrono::system_clock::now().time_since_epoch().count());
      event.timestamp = std::chrono::system_clock::now();

      if (service.updateOrderStatus(event)) {
        std::cout << "✓ Order status updated!" << std::endl;
      }
      break;
    }
    case 3: {
      std::string order_id;
      std::cout << "Enter order ID: ";
      std::cin >> order_id;

      OrderCancellationEvent event;
      event.order_id = order_id;
      event.reason = "Customer request";
      event.transaction_id =
          "TX-" +
          std::to_string(
              std::chrono::system_clock::now().time_since_epoch().count());
      event.timestamp = std::chrono::system_clock::now();

      if (service.cancelOrder(event)) {
        std::cout << "✓ Order cancelled!" << std::endl;
      }
      break;
    }
    }
  }

  std::cout << "Producer service stopped." << std::endl;
  return 0;
}
