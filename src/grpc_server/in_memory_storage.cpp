#include "in_memory_storage.h"

std::string InMemoryStorage::AddProduct(const Product &product) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string id = "prod_" + std::to_string(products_.size() + 1);

  ProductData data;
  data.product = product;
  if (product.id().empty()) {
    data.product.set_id(id);
  } else {
    id = product.id();
  }

  // Copy category ids
  for (int i = 0; i < product.category_ids_size(); i++) {
    data.categories.push_back(product.category_ids(i));
  }

  products_[id] = data;
  return id;
}

bool InMemoryStorage::EditProduct(const Product &product) {
  if (product.id().empty())
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  auto it = products_.find(product.id());
  if (it == products_.end())
    return false;

  // Update product fields but preserve categories
  ProductData &data = it->second;
  data.product.set_name(product.name());
  data.product.set_description(product.description());
  data.product.set_price(product.price());
  data.product.set_quantity(product.quantity());

  // Clear and update category IDs
  data.categories.clear();
  data.product.clear_category_ids();
  for (int i = 0; i < product.category_ids_size(); i++) {
    std::string cat_id = product.category_ids(i);
    data.categories.push_back(cat_id);
    data.product.add_category_ids(cat_id);
  }

  return true;
}

bool InMemoryStorage::DeleteProduct(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = products_.find(id);
  if (it == products_.end())
    return false;

  // Remove product from all categories
  for (const auto &cat_id : it->second.categories) {
    auto cat_it = categories_.find(cat_id);
    if (cat_it != categories_.end()) {
      auto &products = cat_it->second.products;
      products.erase(std::remove(products.begin(), products.end(), id),
                     products.end());

      // Update category proto
      cat_it->second.category.clear_product_ids();
      for (const auto &pid : products) {
        cat_it->second.category.add_product_ids(pid);
      }
    }
  }

  products_.erase(it);
  return true;
}

// Category operations
std::string InMemoryStorage::AddCategory(const Category &category) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string id = "cat_" + std::to_string(categories_.size() + 1);

  CategoryData data;
  data.category = category;
  if (category.id().empty()) {
    data.category.set_id(id);
  } else {
    id = category.id();
  }

  // Copy product ids
  for (int i = 0; i < category.product_ids_size(); i++) {
    data.products.push_back(category.product_ids(i));
  }

  categories_[id] = data;
  return id;
}

bool InMemoryStorage::EditCategory(const Category &category) {
  if (category.id().empty())
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  auto it = categories_.find(category.id());
  if (it == categories_.end())
    return false;

  CategoryData &data = it->second;
  data.category.set_name(category.name());

  // Clear and update product IDs
  data.products.clear();
  data.category.clear_product_ids();
  for (int i = 0; i < category.product_ids_size(); i++) {
    std::string prod_id = category.product_ids(i);
    data.products.push_back(prod_id);
    data.category.add_product_ids(prod_id);
  }

  return true;
}

bool InMemoryStorage::DeleteCategory(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = categories_.find(id);
  if (it == categories_.end())
    return false;

  // Remove category from all products
  for (const auto &prod_id : it->second.products) {
    auto prod_it = products_.find(prod_id);
    if (prod_it != products_.end()) {
      auto &categories = prod_it->second.categories;
      categories.erase(std::remove(categories.begin(), categories.end(), id),
                       categories.end());

      // Update product proto
      prod_it->second.product.clear_category_ids();
      for (const auto &cid : categories) {
        prod_it->second.product.add_category_ids(cid);
      }
    }
  }

  categories_.erase(it);
  return true;
}

// Attachment operations
bool InMemoryStorage::AttachProductToCategory(const std::string &product_id,
                                              const std::string &category_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto prod_it = products_.find(product_id);
  auto cat_it = categories_.find(category_id);

  if (prod_it == products_.end() || cat_it == categories_.end()) {
    return false;
  }

  // Check if already attached
  auto &prod_categories = prod_it->second.categories;
  if (std::find(prod_categories.begin(), prod_categories.end(), category_id) !=
      prod_categories.end()) {
    return true; // Already attached
  }

  // Attach product to category
  prod_categories.push_back(category_id);
  prod_it->second.product.add_category_ids(category_id);

  // Attach category to product
  auto &cat_products = cat_it->second.products;
  cat_products.push_back(product_id);
  cat_it->second.category.add_product_ids(product_id);

  return true;
}

bool InMemoryStorage::DetachProductFromCategory(
    const std::string &product_id, const std::string &category_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto prod_it = products_.find(product_id);
  auto cat_it = categories_.find(category_id);

  if (prod_it == products_.end() || cat_it == categories_.end()) {
    return false;
  }

  // Remove from product's categories
  auto &prod_categories = prod_it->second.categories;
  auto cat_pos =
      std::find(prod_categories.begin(), prod_categories.end(), category_id);
  if (cat_pos != prod_categories.end()) {
    prod_categories.erase(cat_pos);

    // Update product proto
    prod_it->second.product.clear_category_ids();
    for (const auto &cid : prod_categories) {
      prod_it->second.product.add_category_ids(cid);
    }
  }

  // Remove from category's products
  auto &cat_products = cat_it->second.products;
  auto prod_pos =
      std::find(cat_products.begin(), cat_products.end(), product_id);
  if (prod_pos != cat_products.end()) {
    cat_products.erase(prod_pos);

    // Update category proto
    cat_it->second.category.clear_product_ids();
    for (const auto &pid : cat_products) {
      cat_it->second.category.add_product_ids(pid);
    }
  }

  return true;
}

// Get operations
std::vector<Product>
InMemoryStorage::GetAllProducts(const ProductFilter &filter,
                                const Pagination &pagination) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Product> result;

  for (const auto &[id, data] : products_) {
    // Apply filter
    if (filter.max_price() > 0 && data.product.price() > filter.max_price()) {
      continue;
    }
    result.push_back(data.product);
  }

  // Apply pagination
  int offset = pagination.offset();
  int limit = pagination.limit();

  if (offset < 0)
    offset = 0;
  if (offset >= static_cast<int>(result.size()))
    return {};

  if (limit <= 0)
    limit = static_cast<int>(result.size()) - offset;

  size_t end = std::min(result.size(), static_cast<size_t>(offset + limit));
  return std::vector<Product>(result.begin() + offset, result.begin() + end);
}

std::vector<Category>
InMemoryStorage::GetAllCategories(const Pagination &pagination) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Category> result;

  for (const auto &[id, data] : categories_) {
    result.push_back(data.category);
  }

  // Apply pagination
  int offset = pagination.offset();
  int limit = pagination.limit();

  if (offset < 0)
    offset = 0;
  if (offset >= static_cast<int>(result.size()))
    return {};

  if (limit <= 0)
    limit = static_cast<int>(result.size()) - offset;

  size_t end = std::min(result.size(), static_cast<size_t>(offset + limit));
  return std::vector<Category>(result.begin() + offset, result.begin() + end);
}

std::optional<Product> InMemoryStorage::GetProductById(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = products_.find(id);
  if (it == products_.end())
    return std::nullopt;
  return it->second.product;
}

std::optional<Category>
InMemoryStorage::GetCategoryById(const std::string &id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = categories_.find(id);
  if (it == categories_.end())
    return std::nullopt;
  return it->second.category;
}