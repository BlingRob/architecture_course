#include "server.h"

namespace JsonRpc {

Server::Server(net::io_context &ioc, const tcp::endpoint &endpoint,
               const std::string &version, Logging::Logger &logger)
    : ioc_(ioc), acceptor_(ioc), version_(version), logger_(logger) {

  beast::error_code ec;

  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    throw std::runtime_error("Failed to open acceptor: " + ec.message());
  }

  acceptor_.set_option(net::socket_base::reuse_address(true), ec);
  if (ec) {
    throw std::runtime_error("Failed to set socket option: " + ec.message());
  }

  acceptor_.bind(endpoint, ec);
  if (ec) {
    throw std::runtime_error("Failed to bind: " + ec.message());
  }

  acceptor_.listen(net::socket_base::max_listen_connections, ec);
  if (ec) {
    throw std::runtime_error("Failed to listen: " + ec.message());
  }
}

MethodRegistry &Server::GetRegistry() { return registry_; }

void Server::Run() { doAccept(); }

void Server::doAccept() {
  acceptor_.async_accept(net::make_strand(ioc_),
                         beast::bind_front_handler(&Server::onAccept, this));
}

void Server::onAccept(beast::error_code ec, tcp::socket socket) {
  if (ec) {
    LOG_ERROR(logger_.get(), "Accept error: {}", ec.message());
  } else {
    std::make_shared<Session>(std::move(socket), registry_, version_, logger_)
        ->Run();
  }

  doAccept();
}

} // namespace JsonRpc
