#include "auction_manager.h"

#include <algorithm>
#include <chrono>

namespace Auction {

Auction::UserResult AuctionManager::CreateUser(const std::string &name,
                                               double initial_balance) {
  std::lock_guard<std::mutex> lock(mutex_);

  UserResult result;

  if (name.empty()) {
    result.ok = false;
    result.error = "name must be non-empty";
    return result;
  }

  if (initial_balance < 0.0) {
    result.ok = false;
    result.error = "initial balance must be >= 0";
    return result;
  }

  User user;
  user.id = next_user_id_++;
  user.name = name;
  user.balance = initial_balance;

  users_[user.id] = user;

  result.ok = true;
  result.user = user;
  return result;
}

Auction::UserResult AuctionManager::UpdateBalance(int64_t user_id,
                                                  double delta) {
  std::lock_guard<std::mutex> lock(mutex_);

  UserResult result;

  auto it = users_.find(user_id);
  if (it == users_.end()) {
    result.ok = false;
    result.error = "user not found";
    return result;
  }

  const double updated = it->second.balance + delta;
  if (updated < 0.0) {
    result.ok = false;
    result.error = "insufficient balance for update";
    return result;
  }

  it->second.balance = updated;

  result.ok = true;
  result.user = it->second;
  return result;
}

Auction::ItemResult AuctionManager::AddItem(const std::string &name,
                                            double starting_bid) {
  std::lock_guard<std::mutex> lock(mutex_);

  ItemResult result;

  if (name.empty()) {
    result.ok = false;
    result.error = "name must be non-empty";
    return result;
  }

  if (starting_bid <= 0.0) {
    result.ok = false;
    result.error = "starting bid must be > 0";
    return result;
  }

  Item item;
  item.id = next_item_id_++;
  item.name = name;
  item.starting_bid = starting_bid;
  item.current_bid = starting_bid;
  item.status = ItemStatus::Draft;

  items_[item.id] = item;

  result.ok = true;
  result.item = item;
  return result;
}

Auction::ItemResult AuctionManager::StartAuction(int64_t item_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  ItemResult result;

  auto it = items_.find(item_id);
  if (it == items_.end()) {
    result.ok = false;
    result.error = "item not found";
    return result;
  }

  if (it->second.status != ItemStatus::Draft) {
    result.ok = false;
    result.error = "auction can be started only from draft";
    return result;
  }

  it->second.status = ItemStatus::Active;

  result.ok = true;
  result.item = it->second;
  return result;
}

Auction::EndAuctionResult AuctionManager::EndAuction(int64_t item_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  EndAuctionResult result;

  auto it = items_.find(item_id);
  if (it == items_.end()) {
    result.ok = false;
    result.error = "item not found";
    return result;
  }

  auto &item = it->second;
  if (item.status != ItemStatus::Active) {
    result.ok = false;
    result.error = "auction is not active";
    return result;
  }

  result.ok = true;

  if (!item.highest_bidder_id.has_value()) {
    item.status = ItemStatus::ClosedNoBids;
    result.item = item;
    result.final_price = item.current_bid;
    return result;
  }

  const int64_t winner_id = *item.highest_bidder_id;
  auto user_it = users_.find(winner_id);
  if (user_it == users_.end()) {
    result.ok = false;
    result.error = "winner user does not exist (internal state error)";
    return result;
  }

  if (user_it->second.balance < item.current_bid) {
    result.ok = false;
    result.error = "winner has insufficient balance at auction end";
    return result;
  }

  user_it->second.balance -= item.current_bid;
  item.status = ItemStatus::Sold;

  result.item = item;
  result.winner_id = winner_id;
  result.final_price = item.current_bid;
  return result;
}

Auction::ItemResult AuctionManager::PlaceBid(int64_t item_id, int64_t user_id,
                                             double amount) {
  std::lock_guard<std::mutex> lock(mutex_);

  ItemResult result;

  auto item_it = items_.find(item_id);
  if (item_it == items_.end()) {
    result.ok = false;
    result.error = "item not found";
    return result;
  }

  auto user_it = users_.find(user_id);
  if (user_it == users_.end()) {
    result.ok = false;
    result.error = "user not found";
    return result;
  }

  auto &item = item_it->second;
  if (item.status != ItemStatus::Active) {
    result.ok = false;
    result.error = "auction is not active";
    return result;
  }

  if (!IsBetterBid(amount, item.current_bid)) {
    result.ok = false;
    result.error = "bid must be greater than current bid";
    return result;
  }

  if (user_it->second.balance < amount) {
    result.ok = false;
    result.error = "insufficient balance";
    return result;
  }

  item.current_bid = amount;
  item.highest_bidder_id = user_id;

  Bid bid;
  bid.user_id = user_id;
  bid.amount = amount;
  bid.timestamp_ms = NowUnixMs();
  item.bid_history.push_back(bid);

  ApplyAutoBids(item, user_id);

  result.ok = true;
  result.item = item;
  return result;
}

Auction::ItemResult AuctionManager::SetAutoBid(int64_t item_id, int64_t user_id,
                                                double max_amount) {
  std::lock_guard<std::mutex> lock(mutex_);

  ItemResult result;

  auto item_it = items_.find(item_id);
  if (item_it == items_.end()) {
    result.ok = false;
    result.error = "item not found";
    return result;
  }

  auto user_it = users_.find(user_id);
  if (user_it == users_.end()) {
    result.ok = false;
    result.error = "user not found";
    return result;
  }

  auto &item = item_it->second;
  if (item.status != ItemStatus::Active) {
    result.ok = false;
    result.error = "auction is not active";
    return result;
  }

  if (max_amount <= item.current_bid) {
    result.ok = false;
    result.error = "auto bid max must be greater than current bid";
    return result;
  }

  if (user_it->second.balance < max_amount) {
    result.ok = false;
    result.error = "insufficient balance for auto bid max";
    return result;
  }

  item.auto_bid_limits[user_id] = max_amount;
  ApplyAutoBids(item, user_id);

  result.ok = true;
  result.item = item;
  return result;
}

std::vector<User> AuctionManager::GetUsers() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<User> result;
  result.reserve(users_.size());
  for (const auto &[_, user] : users_) {
    result.push_back(user);
  }

  std::sort(result.begin(), result.end(),
            [](const User &a, const User &b) { return a.id < b.id; });

  return result;
}

std::vector<Item> AuctionManager::GetItems() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<Item> result;
  result.reserve(items_.size());
  for (const auto &[_, item] : items_) {
    result.push_back(item);
  }

  std::sort(result.begin(), result.end(),
            [](const Item &a, const Item &b) { return a.id < b.id; });

  return result;
}

int64_t AuctionManager::NowUnixMs() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
      .count();
}

bool AuctionManager::IsBetterBid(double amount, double current_bid) {
  return amount > current_bid;
}

std::string AuctionManager::ToLower(std::string value) { return value; }

void AuctionManager::ApplyAutoBids(Item &item, int64_t triggering_user_id) {
  std::vector<int64_t> users_order;
  users_order.reserve(item.auto_bid_limits.size());
  for (const auto &[user_id, _] : item.auto_bid_limits) {
    users_order.push_back(user_id);
  }
  std::sort(users_order.begin(), users_order.end());

  bool progressed = true;
  while (progressed) {
    progressed = false;

    for (const int64_t auto_user_id : users_order) {
      if (item.highest_bidder_id.has_value() &&
          *item.highest_bidder_id == auto_user_id) {
        continue;
      }

      auto limit_it = item.auto_bid_limits.find(auto_user_id);
      if (limit_it == item.auto_bid_limits.end()) {
        continue;
      }

      auto user_it = users_.find(auto_user_id);
      if (user_it == users_.end()) {
        continue;
      }

      const double max_amount = limit_it->second;
      const double next_amount = item.current_bid + min_bid_step_;
      const double effective_limit = std::min(max_amount, user_it->second.balance);

      if (effective_limit >= next_amount) {
        item.current_bid = next_amount;
        item.highest_bidder_id = auto_user_id;

        Bid bid;
        bid.user_id = auto_user_id;
        bid.amount = next_amount;
        bid.timestamp_ms = NowUnixMs();
        item.bid_history.push_back(bid);

        progressed = true;
      }
    }
  }

  (void)triggering_user_id;
}

} // namespace Auction
