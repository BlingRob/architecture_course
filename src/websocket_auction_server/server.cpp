#include "server.h"
#include "session.h"

namespace AuctionWs {

Server::Server(net::io_context &ioc, const tcp::endpoint &endpoint,
               Auction::AuctionManager &manager, Logging::Logger &logger)
    : ioc_(ioc), acceptor_(ioc), manager_(manager), logger_(logger) {
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
    throw std::runtime_error("Failed to bind endpoint: " + ec.message());
  }

  acceptor_.listen(net::socket_base::max_listen_connections, ec);
  if (ec) {
    throw std::runtime_error("Failed to listen: " + ec.message());
  }
}

void Server::Run() { DoAccept(); }

void Server::RegisterSession(const std::shared_ptr<Session> &session) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  sessions_.insert(session);
}

void Server::UnregisterSession(const std::shared_ptr<Session> &session) {
  std::lock_guard<std::mutex> lock(sessions_mutex_);
  sessions_.erase(session);
}

void Server::Broadcast(const json &message) {
  std::vector<std::shared_ptr<Session>> snapshot;
  {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    snapshot.reserve(sessions_.size());
    for (const auto &session : sessions_) {
      snapshot.push_back(session);
    }
  }

  for (const auto &session : snapshot) {
    session->Deliver(message);
  }
}

Auction::AuctionManager &Server::GetManager() { return manager_; }

Logging::Logger &Server::GetLogger() { return logger_; }

void Server::DoAccept() {
  acceptor_.async_accept(net::make_strand(ioc_),
                         beast::bind_front_handler(&Server::OnAccept, this));
}

void Server::OnAccept(beast::error_code ec, tcp::socket socket) {
  if (ec) {
    LOG_ERROR(logger_.get(), "Accept error: {}", ec.message());
  } else {
    auto session = std::make_shared<Session>(std::move(socket), *this);
    RegisterSession(session);
    session->Run();
  }

  DoAccept();
}

} // namespace AuctionWs
