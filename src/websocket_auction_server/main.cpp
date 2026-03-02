#include "auction_manager.h"
#include "logger.h"
#include "server.h"

#include <boost/asio.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
#include <toml.hpp>

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

int main() {
  try {
    toml::table cfg;

    const std::string config_file_path{"cfg.toml"};
    if (!std::filesystem::exists(config_file_path)) {
      throw std::runtime_error("Config file not found: " + config_file_path);
    }

    cfg = toml::parse_file(config_file_path);
    Logging::LoggerFactory::Init(cfg);

    auto logger = Logging::LoggerFactory::GetLogger(
        cfg["logging"]["filename"].value_or("websocket_auction_server.log"));

    const std::string address{
        cfg["server_parameters"]["host"].value_or("0.0.0.0")};
    const unsigned short port{static_cast<unsigned short>(
        cfg["server_parameters"]["port"].value_or(18080))};
    const int threads{cfg["server_parameters"]["threads"].value_or(2)};

    net::io_context ioc(threads);
    auto endpoint = tcp::endpoint(net::ip::make_address(address), port);

    Auction::AuctionManager manager;
    AuctionWs::Server server(ioc, endpoint, manager, logger);

    LOG_INFO(logger.get(), "Starting WebSocket Auction server on {}:{}", address,
             port);

    server.Run();

    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(std::max(0, threads - 1)));
    for (int i = 0; i < threads - 1; ++i) {
      workers.emplace_back([&ioc]() { ioc.run(); });
    }

    ioc.run();

    for (auto &worker : workers) {
      worker.join();
    }

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
