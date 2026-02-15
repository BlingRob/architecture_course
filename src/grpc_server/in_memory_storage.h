#pragma once

#include "ecommerce.grpc.pb.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using ecommerce::Category;
using ecommerce::Pagination;
using ecommerce::Product;
using ecommerce::ProductFilter;

class InMemoryStorage {
public:
  // Product operations
  std::string AddProduct(const Product &product);

  bool EditProduct(const Product &product);

  bool DeleteProduct(const std::string &id);

  // Category operations
  std::string AddCategory(const Category &category);

  bool EditCategory(const Category &category);

  bool DeleteCategory(const std::string &id);

  // Attachment operations
  bool AttachProductToCategory(const std::string &product_id,
                               const std::string &category_id);

  bool DetachProductFromCategory(const std::string &product_id,
                                 const std::string &category_id);

  // Get operations
  std::vector<Product> GetAllProducts(const ProductFilter &filter,
                                      const Pagination &pagination);

  std::vector<Category> GetAllCategories(const Pagination &pagination);

  std::optional<Product> GetProductById(const std::string &id);

  std::optional<Category> GetCategoryById(const std::string &id);

private:
  struct ProductData {
    Product product;
    std::vector<std::string> categories;
  };

  struct CategoryData {
    Category category;
    std::vector<std::string> products;
  };

private:
  std::unordered_map<std::string, ProductData> products_;
  std::unordered_map<std::string, CategoryData> categories_;
  mutable std::mutex mutex_;
};