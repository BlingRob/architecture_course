#include "session.h"

#include <algorithm>

namespace AuctionWs {

Session::Session(tcp::socket &&socket, Server &server)
    : ws_(std::move(socket)), server_(server) {}

void Session::Run() {
  ws_.set_option(
      websocket::stream_base::timeout::suggested(beast::role_type::server));

  ws_.set_option(
      websocket::stream_base::decorator([](websocket::response_type &res) {
        res.set(http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) + " Auction WS Server");
      }));

  ws_.async_accept(
      beast::bind_front_handler(&Session::OnAccept, shared_from_this()));
}

void Session::Deliver(const json &message) { QueueWrite(message.dump()); }

void Session::OnAccept(beast::error_code ec) {
  if (ec) {
    LOG_ERROR(server_.GetLogger().get(), "WebSocket accept error: {}", ec.message());
    server_.UnregisterSession(shared_from_this());
    return;
  }

  DoRead();
}

void Session::DoRead() {
  ws_.async_read(
      buffer_, beast::bind_front_handler(&Session::OnRead, shared_from_this()));
}

void Session::OnRead(beast::error_code ec, std::size_t bytes_transferred) {
  (void)bytes_transferred;

  if (ec == websocket::error::closed) {
    server_.UnregisterSession(shared_from_this());
    return;
  }

  if (ec) {
    LOG_ERROR(server_.GetLogger().get(), "WebSocket read error: {}", ec.message());
    server_.UnregisterSession(shared_from_this());
    return;
  }

  try {
    const std::string payload = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());

    const auto request = json::parse(payload);
    const auto response = HandleRequest(request);
    Deliver(response);
  } catch (const std::exception &e) {
    Deliver(BuildError("unknown", std::string("invalid request: ") + e.what()));
  }

  DoRead();
}

void Session::QueueWrite(std::string payload) {
  net::post(ws_.get_executor(),
            [self = shared_from_this(), payload = std::move(payload)]() mutable {
              const bool writing = !self->write_queue_.empty();
              self->write_queue_.push_back(std::move(payload));
              if (!writing) {
                self->DoWrite();
              }
            });
}

void Session::DoWrite() {
  ws_.text(true);
  ws_.async_write(net::buffer(write_queue_.front()),
                  beast::bind_front_handler(&Session::OnWrite,
                                            shared_from_this()));
}

void Session::OnWrite(beast::error_code ec, std::size_t bytes_transferred) {
  (void)bytes_transferred;

  if (ec) {
    LOG_ERROR(server_.GetLogger().get(), "WebSocket write error: {}", ec.message());
    server_.UnregisterSession(shared_from_this());
    return;
  }

  write_queue_.pop_front();
  if (!write_queue_.empty()) {
    DoWrite();
  }
}

json Session::HandleRequest(const json &request) {
  if (!request.is_object() || !request.contains("action") ||
      !request["action"].is_string()) {
    return BuildError("unknown", "field 'action' is required");
  }

  const std::string action = request["action"].get<std::string>();
  return HandleAction(action, request);
}

json Session::HandleAction(const std::string &action, const json &request) {
  auto &manager = server_.GetManager();

  if (action == "create_user") {
    if (!request.contains("name") || !request["name"].is_string()) {
      return BuildError(action, "field 'name' is required");
    }

    const std::string name = request["name"].get<std::string>();
    const double balance =
        request.contains("balance") && request["balance"].is_number()
            ? request["balance"].get<double>()
            : 0.0;

    const auto result = manager.CreateUser(name, balance);
    if (!result.ok) {
      return BuildError(action, result.error);
    }

    const json payload = {{"user", UserToJson(result.user)}};
    server_.Broadcast({{"type", "event"},
                       {"event", "user_created"},
                       {"user", UserToJson(result.user)}});
    return BuildSuccess(action, payload);
  }

  if (action == "update_balance") {
    if (!request.contains("userId") || !request["userId"].is_number_integer() ||
        !request.contains("delta") || !request["delta"].is_number()) {
      return BuildError(action, "fields 'userId' and 'delta' are required");
    }

    const auto result = manager.UpdateBalance(request["userId"].get<int64_t>(),
                                              request["delta"].get<double>());
    if (!result.ok) {
      return BuildError(action, result.error);
    }

    server_.Broadcast({{"type", "event"},
                       {"event", "balance_updated"},
                       {"user", UserToJson(result.user)}});
    return BuildSuccess(action, {{"user", UserToJson(result.user)}});
  }

  if (action == "add_item") {
    if (!request.contains("name") || !request["name"].is_string() ||
        !request.contains("startingBid") || !request["startingBid"].is_number()) {
      return BuildError(action, "fields 'name' and 'startingBid' are required");
    }

    const auto result = manager.AddItem(request["name"].get<std::string>(),
                                        request["startingBid"].get<double>());
    if (!result.ok) {
      return BuildError(action, result.error);
    }

    server_.Broadcast({{"type", "event"},
                       {"event", "item_added"},
                       {"item", ItemToJson(result.item)}});
    return BuildSuccess(action, {{"item", ItemToJson(result.item)}});
  }

  if (action == "start_auction") {
    if (!request.contains("itemId") || !request["itemId"].is_number_integer()) {
      return BuildError(action, "field 'itemId' is required");
    }

    const auto result = manager.StartAuction(request["itemId"].get<int64_t>());
    if (!result.ok) {
      return BuildError(action, result.error);
    }

    server_.Broadcast({{"type", "event"},
                       {"event", "auction_started"},
                       {"item", ItemToJson(result.item)}});
    return BuildSuccess(action, {{"item", ItemToJson(result.item)}});
  }

  if (action == "end_auction") {
    if (!request.contains("itemId") || !request["itemId"].is_number_integer()) {
      return BuildError(action, "field 'itemId' is required");
    }

    const auto result = manager.EndAuction(request["itemId"].get<int64_t>());
    if (!result.ok) {
      return BuildError(action, result.error);
    }

    server_.Broadcast({{"type", "event"},
                       {"event", "auction_ended"},
                       {"item", ItemToJson(result.item)},
                       {"winnerId", result.winner_id.has_value() ? json(*result.winner_id) : json()},
                       {"finalPrice", result.final_price}});

    return BuildSuccess(action,
                        {{"item", ItemToJson(result.item)},
                         {"winnerId", result.winner_id.has_value() ? json(*result.winner_id) : json()},
                         {"finalPrice", result.final_price}});
  }

  if (action == "place_bid") {
    if (!request.contains("itemId") || !request["itemId"].is_number_integer() ||
        !request.contains("userId") || !request["userId"].is_number_integer() ||
        !request.contains("amount") || !request["amount"].is_number()) {
      return BuildError(action,
                        "fields 'itemId', 'userId', 'amount' are required");
    }

    const auto result = manager.PlaceBid(request["itemId"].get<int64_t>(),
                                         request["userId"].get<int64_t>(),
                                         request["amount"].get<double>());
    if (!result.ok) {
      return BuildError(action, result.error);
    }

    server_.Broadcast({{"type", "event"},
                       {"event", "bid_updated"},
                       {"item", ItemToJson(result.item)}});
    return BuildSuccess(action, {{"item", ItemToJson(result.item)}});
  }

  if (action == "set_auto_bid") {
    if (!request.contains("itemId") || !request["itemId"].is_number_integer() ||
        !request.contains("userId") || !request["userId"].is_number_integer() ||
        !request.contains("maxAmount") || !request["maxAmount"].is_number()) {
      return BuildError(action,
                        "fields 'itemId', 'userId', 'maxAmount' are required");
    }

    const auto result = manager.SetAutoBid(request["itemId"].get<int64_t>(),
                                           request["userId"].get<int64_t>(),
                                           request["maxAmount"].get<double>());
    if (!result.ok) {
      return BuildError(action, result.error);
    }

    server_.Broadcast({{"type", "event"},
                       {"event", "auto_bid_set"},
                       {"item", ItemToJson(result.item)}});
    return BuildSuccess(action, {{"item", ItemToJson(result.item)}});
  }

  if (action == "get_state") {
    const auto users = manager.GetUsers();
    const auto items = manager.GetItems();

    json users_json = json::array();
    for (const auto &user : users) {
      users_json.push_back(UserToJson(user));
    }

    json items_json = json::array();
    for (const auto &item : items) {
      items_json.push_back(ItemToJson(item));
    }

    return BuildSuccess(action, {{"users", users_json}, {"items", items_json}});
  }

  return BuildError(action, "unknown action");
}

json Session::BuildSuccess(const std::string &action, const json &payload) {
  json response = {{"type", "response"},
                   {"action", action},
                   {"ok", true}};

  if (payload.is_object()) {
    for (auto it = payload.begin(); it != payload.end(); ++it) {
      response[it.key()] = it.value();
    }
  }

  return response;
}

json Session::BuildError(const std::string &action,
                        const std::string &error_message) {
  return {{"type", "response"},
          {"action", action},
          {"ok", false},
          {"error", error_message}};
}

json Session::UserToJson(const Auction::User &user) {
  return {{"id", user.id}, {"name", user.name}, {"balance", user.balance}};
}

json Session::ItemToJson(const Auction::Item &item) {
  json bid_history = json::array();
  for (const auto &bid : item.bid_history) {
    bid_history.push_back({{"userId", bid.user_id},
                           {"amount", bid.amount},
                           {"timestampMs", bid.timestamp_ms}});
  }

  json auto_bids = json::array();
  for (const auto &[user_id, limit] : item.auto_bid_limits) {
    auto_bids.push_back({{"userId", user_id}, {"maxAmount", limit}});
  }

  return {{"id", item.id},
          {"name", item.name},
          {"startingBid", item.starting_bid},
          {"currentBid", item.current_bid},
          {"status", StatusToString(item.status)},
          {"highestBidderId",
           item.highest_bidder_id.has_value() ? json(*item.highest_bidder_id)
                                              : json()},
          {"autoBids", auto_bids},
          {"bidHistory", bid_history}};
}

std::string Session::StatusToString(Auction::ItemStatus status) {
  switch (status) {
  case Auction::ItemStatus::Draft:
    return "draft";
  case Auction::ItemStatus::Active:
    return "active";
  case Auction::ItemStatus::Sold:
    return "sold";
  case Auction::ItemStatus::ClosedNoBids:
    return "closed_no_bids";
  }

  return "unknown";
}

} // namespace AuctionWs
