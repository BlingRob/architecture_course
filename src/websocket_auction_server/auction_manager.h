#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Auction {

enum class ItemStatus { Draft, Active, Sold, ClosedNoBids };

struct User {
  int64_t id{};
  std::string name;
  double balance{};
};

struct Bid {
  int64_t user_id{};
  double amount{};
  int64_t timestamp_ms{};
};

struct Item {
  int64_t id{};
  std::string name;
  double starting_bid{};
  double current_bid{};
  ItemStatus status{ItemStatus::Draft};
  std::optional<int64_t> highest_bidder_id;
  std::vector<Bid> bid_history;
  std::unordered_map<int64_t, double> auto_bid_limits;
};

struct OperationResult {
  bool ok{false};
  std::string error;
};

struct UserResult : OperationResult {
  User user;
};

struct ItemResult : OperationResult {
  Item item;
};

struct EndAuctionResult : OperationResult {
  Item item;
  std::optional<int64_t> winner_id;
  double final_price{};
};

class AuctionManager {
public:
  UserResult CreateUser(const std::string &name, double initial_balance);
  UserResult UpdateBalance(int64_t user_id, double delta);

  ItemResult AddItem(const std::string &name, double starting_bid);
  ItemResult StartAuction(int64_t item_id);
  EndAuctionResult EndAuction(int64_t item_id);

  ItemResult PlaceBid(int64_t item_id, int64_t user_id, double amount);
  ItemResult SetAutoBid(int64_t item_id, int64_t user_id, double max_amount);

  std::vector<User> GetUsers() const;
  std::vector<Item> GetItems() const;

private:
  static int64_t NowUnixMs();
  static bool IsBetterBid(double amount, double current_bid);
  static std::string ToLower(std::string value);

  void ApplyAutoBids(Item &item, int64_t triggering_user_id);

private:
  mutable std::mutex mutex_;
  std::unordered_map<int64_t, User> users_;
  std::unordered_map<int64_t, Item> items_;
  int64_t next_user_id_{1};
  int64_t next_item_id_{1};
  double min_bid_step_{1.0};
};

} // namespace Auction
