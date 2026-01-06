#include "json_rpc_server.h"
#include "logger.h"
#include "secret_data_manager.h"

#include <format>
#include <iostream>
#include <memory>
#include <toml.hpp>

int main(int argc, char *argv[]) {
  try {
    toml::table cfg;

    const std::string config_file_path{"cfg.toml"};

    if (!std::filesystem::exists(config_file_path)) {
      throw std::runtime_error("Config file not found: " + config_file_path);
    }

    cfg = toml::parse_file(config_file_path);

    Logging::LoggerFactory::Init(cfg);

    auto logger{Logging::LoggerFactory::GetLogger(
        cfg["logging"]["filename"].value_or("json_rpc_server.log"))};

    // Конфигурация
    const std::string address{
        cfg["server_parameters"]["host"].value_or("0.0.0.0")};
    const unsigned short port{static_cast<unsigned short>(
        cfg["server_parameters"]["port"].value_or(8080))};
    const std::string version{
        cfg["server_parameters"]["version"].value_or("v1")};
    const int threads{cfg["server_parameters"]["threads"].value_or(1)};

    net::io_context ioc(threads);

    auto const endpoint{tcp::endpoint(net::ip::make_address(address), port)};

    LOG_INFO(logger.get(), "Creating server...");
    JsonRpc::Server server(ioc, endpoint, version, logger);
    LOG_INFO(logger.get(), "Server created");

    LOG_INFO(logger.get(), "Creating secret data manager...");
    SecretData::SecretDataManager data_manager;
    LOG_INFO(logger.get(), "Secret data manager created");

    LOG_INFO(logger.get(),
             "Server {}:{} with version: {} on {} threads was started", address,
             port, version, threads);
    data_manager.RegisterMethods(server.get_registry());

    server.Run();

    // Запускаем потоки для обработки
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i) {
      v.emplace_back([&ioc] { ioc.run(); });
    }

    ioc.run();

    // Ждем завершения всех потоков
    for (auto &t : v) {
      t.join();
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}