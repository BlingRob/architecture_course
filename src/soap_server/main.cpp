#include "logger.h"
#include "soap_server.h"
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <toml.hpp>

int main() {
  try {

    toml::table cfg;

    const std::string config_file_path{"cfg.toml"};

    if (!std::filesystem::exists(config_file_path)) {
      throw std::runtime_error("Config file not found: " + config_file_path);
    }

    cfg = toml::parse_file(config_file_path);

    Logging::LoggerFactory::Init(cfg);

    auto logger{Logging::LoggerFactory::GetLogger(
        cfg["logging"]["filename"].value_or("soap_server.log"))};

    net::io_context ioc;
    boost::asio::signal_set sig_set(ioc, SIGINT, SIGTERM);

    // Конфигурация
    const std::string address{
        cfg["server_parameters"]["host"].value_or("0.0.0.0")};
    const unsigned short port{static_cast<unsigned short>(
        cfg["server_parameters"]["port"].value_or(8080))};

    LOG_INFO(logger.get(), "Creating server...");
    auto const endpoint{tcp::endpoint(net::ip::make_address(address), port)};
    std::unique_ptr<SOAPServer> server{
        std::make_unique<SOAPServer>(ioc, endpoint)};
    LOG_INFO(logger.get(), "Server created");

    sig_set.async_wait([&](auto, auto) {
      if (!server) {
        server->Stop();
      }
      ioc.stop();
    });

    // Запуск сервера
    server->Start();

    LOG_INFO(
        logger.get(),
        "Task Management SOAP Server\n ============================\nServer "
        "started on {}:{}\nWSDL available at: "
        "http://localhost:{}/soap?wsdl\nPress Ctrl+C to stop\n",
        address, port, port);

    // Запуск io_context
    ioc.run();

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}