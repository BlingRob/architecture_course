#pragma once
#include <chrono>
#include <shared_mutex>
#include <unordered_set>


class IdempotencyCache {
public:
  bool contains(const std::string &transaction_id) const {
    std::shared_lock lock(mutex_);
    return cache_.find(transaction_id) != cache_.end();
  }

  void add(const std::string &transaction_id) {
    std::unique_lock lock(mutex_);
    cache_.insert(transaction_id);
    // Очистка старых записей (опционально)
    cleanUp();
  }

private:
  std::unordered_set<std::string> cache_;
  mutable std::shared_mutex mutex_;

  void cleanUp() {
    if (cache_.size() > MAX_CACHE_SIZE) {
      // Простейшая стратегия: очищаем половину
      auto it = cache_.begin();
      for (size_t i = 0; i < MAX_CACHE_SIZE / 2 && it != cache_.end();
           ++i, it = cache_.erase(it))
        ;
    }
  }

  static constexpr size_t MAX_CACHE_SIZE = 10000;
};