#include "ecommerce_service_impl.h"
#include "logger.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <toml.hpp>

using grpc::Server;
using grpc::ServerBuilder;

void RunServer(toml::table &cfg) {

  // Конфигурация
  const std::string address{
      cfg["server_parameters"]["host"].value_or("0.0.0.0")};
  const unsigned short port{static_cast<unsigned short>(
      cfg["server_parameters"]["port"].value_or(15000))};
  auto logger{Logging::LoggerFactory::GetLogger(
      cfg["logging"]["filename"].value_or("grpc_server.log"))};

  std::string server_address(std::format("{}:{}", address, port));
  EcommerceServiceImpl service(logger);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char **argv) {
  try {
    toml::table cfg;

    const std::string config_file_path{"cfg.toml"};

    if (!std::filesystem::exists(config_file_path)) {
      throw std::runtime_error("Config file not found: " + config_file_path);
    }

    cfg = toml::parse_file(config_file_path);

    Logging::LoggerFactory::Init(cfg);

    RunServer(cfg);

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return EXIT_FAILURE;
}