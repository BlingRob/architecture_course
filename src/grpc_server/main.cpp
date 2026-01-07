// #include "test.grpc.pb.h"

// #include <agrpc/asio_grpc.hpp>
// #include <agrpc/server_rpc.hpp>
// #include <boost/asio/use_awaitable.hpp>
// #include <grpcpp/server.h>
// #include <grpcpp/server_builder.h>

// #include <iostream>

// namespace asio = boost::asio;

// namespace example {
// template <auto RequestRPC>
// using AwaitableServerRPC = boost::asio::use_awaitable_t<>::as_default_on_t<
//     agrpc::ServerRPC<RequestRPC>>;

// struct ServerRPCNotifyWhenDoneTraits : agrpc::DefaultServerRPCTraits {
//   static constexpr bool NOTIFY_WHEN_DONE = true;
// };

// template <auto RequestRPC>
// using AwaitableNotifyWhenDoneServerRPC =
//     boost::asio::use_awaitable_t<>::as_default_on_t<
//         agrpc::ServerRPC<RequestRPC, ServerRPCNotifyWhenDoneTraits>>;

// struct RethrowFirstArg {
//   template <class... T> void operator()(std::exception_ptr ep, T &&...) {
//     if (ep) {
//       std::rethrow_exception(ep);
//     }
//   }

//   template <class... T> void operator()(T &&...) {}
// };
// } // namespace example

// int main(int argc, const char **argv) {
//   const auto port = argc >= 2 ? argv[1] : "50051";
//   const auto host = std::string("0.0.0.0:") + port;

//   example::Greeter::AsyncService service;
//   std::unique_ptr<grpc::Server> server;

//   grpc::ServerBuilder builder;
//   agrpc::GrpcContext grpc_context{builder.AddCompletionQueue()};
//   builder.AddListeningPort(host, grpc::InsecureServerCredentials());
//   builder.RegisterService(&service);
//   server = builder.BuildAndStart();

//   using RPC = example::AwaitableServerRPC<
//       &example::Greeter::AsyncService::RequestSayHello>;
//   agrpc::register_awaitable_rpc_handler<RPC>(
//       grpc_context, service,
//       [&](RPC &rpc, example::HelloRequest &request) -> asio::awaitable<void>
//       {
//         example::HelloReply response;
//         response.set_message("Hello " + request.name());
//         std::cerr << "Server get request!" << std::endl;
//         co_await rpc.finish(response, grpc::Status::OK);
//         server->Shutdown();
//       },
//       example::RethrowFirstArg{});

//   grpc_context.run();

//   return 0;
// }

#include "logger.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <toml.hpp>

#include "ecommerce.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using ecommerce::AttachOrDetachRequest;
using ecommerce::Category;
using ecommerce::CategoryListResponse;
using ecommerce::CategoryRequest;
using ecommerce::CategoryResponse;
using ecommerce::Ecommerce;
using ecommerce::Pagination;
using ecommerce::Product;
using ecommerce::ProductFilter;
using ecommerce::ProductListResponse;
using ecommerce::ProductRequest;
using ecommerce::ProductResponse;

class InMemoryStorage {
public:
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

public:
  // Product operations
  std::string AddProduct(const Product &product) {
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

  bool EditProduct(const Product &product) {
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

  bool DeleteProduct(const std::string &id) {
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
  std::string AddCategory(const Category &category) {
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

  bool EditCategory(const Category &category) {
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

  bool DeleteCategory(const std::string &id) {
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
  bool AttachProductToCategory(const std::string &product_id,
                               const std::string &category_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto prod_it = products_.find(product_id);
    auto cat_it = categories_.find(category_id);

    if (prod_it == products_.end() || cat_it == categories_.end()) {
      return false;
    }

    // Check if already attached
    auto &prod_categories = prod_it->second.categories;
    if (std::find(prod_categories.begin(), prod_categories.end(),
                  category_id) != prod_categories.end()) {
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

  bool DetachProductFromCategory(const std::string &product_id,
                                 const std::string &category_id) {
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
  std::vector<Product> GetAllProducts(const ProductFilter &filter,
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

  std::vector<Category> GetAllCategories(const Pagination &pagination) {
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

  std::optional<Product> GetProductById(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = products_.find(id);
    if (it == products_.end())
      return std::nullopt;
    return it->second.product;
  }

  std::optional<Category> GetCategoryById(const std::string &id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = categories_.find(id);
    if (it == categories_.end())
      return std::nullopt;
    return it->second.category;
  }
};

class EcommerceServiceImpl final : public Ecommerce::Service {
private:
  InMemoryStorage storage_;

public:
  Status AddProduct(ServerContext *context, const ProductRequest *request,
                    ProductResponse *response) override {
    std::cout << "AddProduct called" << std::endl;

    if (!request->has_product()) {
      return Status(grpc::INVALID_ARGUMENT, "Product is required");
    }

    std::string id = storage_.AddProduct(request->product());
    auto product = storage_.GetProductById(id);
    if (product) {
      *response->mutable_product() = *product;
      return Status::OK;
    }

    return Status(grpc::INTERNAL, "Failed to add product");
  }

  Status EditProduct(ServerContext *context, const ProductRequest *request,
                     ProductResponse *response) override {
    std::cout << "EditProduct called" << std::endl;

    if (!request->has_product() || request->product().id().empty()) {
      return Status(grpc::INVALID_ARGUMENT, "Product ID is required");
    }

    if (storage_.EditProduct(request->product())) {
      auto product = storage_.GetProductById(request->product().id());
      if (product) {
        *response->mutable_product() = *product;
        return Status::OK;
      }
    }

    return Status(grpc::NOT_FOUND, "Product not found");
  }

  Status DeleteProduct(ServerContext *context, const ProductRequest *request,
                       ProductResponse *response) override {
    std::cout << "DeleteProduct called" << std::endl;

    if (!request->has_product() || request->product().id().empty()) {
      return Status(grpc::INVALID_ARGUMENT, "Product ID is required");
    }

    if (storage_.DeleteProduct(request->product().id())) {
      // For delete operations, we might return the deleted product or empty
      // response
      response->mutable_product()->set_id(request->product().id());
      return Status::OK;
    }

    return Status(grpc::NOT_FOUND, "Product not found");
  }

  Status AddCategory(ServerContext *context, const CategoryRequest *request,
                     CategoryResponse *response) override {
    std::cout << "AddCategory called" << std::endl;

    if (!request->has_category()) {
      return Status(grpc::INVALID_ARGUMENT, "Category is required");
    }

    std::string id = storage_.AddCategory(request->category());
    auto category = storage_.GetCategoryById(id);
    if (category) {
      *response->mutable_category() = *category;
      return Status::OK;
    }

    return Status(grpc::INTERNAL, "Failed to add category");
  }

  Status EditCategory(ServerContext *context, const CategoryRequest *request,
                      CategoryResponse *response) override {
    std::cout << "EditCategory called" << std::endl;

    if (!request->has_category() || request->category().id().empty()) {
      return Status(grpc::INVALID_ARGUMENT, "Category ID is required");
    }

    if (storage_.EditCategory(request->category())) {
      auto category = storage_.GetCategoryById(request->category().id());
      if (category) {
        *response->mutable_category() = *category;
        return Status::OK;
      }
    }

    return Status(grpc::NOT_FOUND, "Category not found");
  }

  Status DeleteCategory(ServerContext *context, const CategoryRequest *request,
                        CategoryResponse *response) override {
    std::cout << "DeleteCategory called" << std::endl;

    if (!request->has_category() || request->category().id().empty()) {
      return Status(grpc::INVALID_ARGUMENT, "Category ID is required");
    }

    if (storage_.DeleteCategory(request->category().id())) {
      response->mutable_category()->set_id(request->category().id());
      return Status::OK;
    }

    return Status(grpc::NOT_FOUND, "Category not found");
  }

  Status AttachProductToCategory(ServerContext *context,
                                 const AttachOrDetachRequest *request,
                                 CategoryResponse *response) override {
    std::cout << "AttachProductToCategory called" << std::endl;

    if (request->product_id().empty() || request->category_id().empty()) {
      return Status(grpc::INVALID_ARGUMENT,
                    "Product ID and Category ID are required");
    }

    if (storage_.AttachProductToCategory(request->product_id(),
                                         request->category_id())) {
      auto category = storage_.GetCategoryById(request->category_id());
      if (category) {
        *response->mutable_category() = *category;
        return Status::OK;
      }
    }

    return Status(grpc::NOT_FOUND, "Product or Category not found");
  }

  Status DetachProductFromCategory(ServerContext *context,
                                   const AttachOrDetachRequest *request,
                                   CategoryResponse *response) override {
    std::cout << "DetachProductFromCategory called" << std::endl;

    if (request->product_id().empty() || request->category_id().empty()) {
      return Status(grpc::INVALID_ARGUMENT,
                    "Product ID and Category ID are required");
    }

    if (storage_.DetachProductFromCategory(request->product_id(),
                                           request->category_id())) {
      auto category = storage_.GetCategoryById(request->category_id());
      if (category) {
        *response->mutable_category() = *category;
        return Status::OK;
      }
    }

    return Status(grpc::NOT_FOUND, "Product or Category not found");
  }

  Status GetAllProducts(ServerContext *context, const ProductRequest *request,
                        ProductListResponse *response) override {
    std::cout << "GetAllProducts called" << std::endl;

    ProductFilter filter;
    Pagination pagination;

    if (request->has_filter()) {
      filter = request->filter();
    }

    if (request->has_pagination()) {
      pagination = request->pagination();
    } else {
      // Default pagination
      pagination.set_limit(100);
      pagination.set_offset(0);
    }

    auto products = storage_.GetAllProducts(filter, pagination);
    for (const auto &product : products) {
      *response->add_products() = product;
    }

    return Status::OK;
  }

  Status GetAllCategories(ServerContext *context,
                          const CategoryRequest *request,
                          CategoryListResponse *response) override {
    std::cout << "GetAllCategories called" << std::endl;

    Pagination pagination;

    if (request->has_category() && !request->category().id().empty()) {
      // Если предоставлен ID, возвращаем одну категорию
      auto category = storage_.GetCategoryById(request->category().id());
      if (category) {
        *response->add_categories() = *category;
      }
      // Возвращаем OK даже если не нашли - пустой список
      return Status::OK;
    } else {
      // Пагинация по умолчанию
      pagination.set_limit(100);
      pagination.set_offset(0);

      auto categories = storage_.GetAllCategories(pagination);
      for (const auto &category : categories) {
        *response->add_categories() = category;
      }
    }

    return Status::OK;
  }

  Status GetProductById(ServerContext *context, const ProductRequest *request,
                        ProductResponse *response) override {
    std::cout << "GetProductById called" << std::endl;

    if (!request->has_product() || request->product().id().empty()) {
      return Status(grpc::INVALID_ARGUMENT, "Product ID is required");
    }

    auto product = storage_.GetProductById(request->product().id());
    if (product) {
      *response->mutable_product() = *product;
      return Status::OK;
    }

    return Status(grpc::NOT_FOUND, "Product not found");
  }

  Status GetCategoryById(ServerContext *context, const CategoryRequest *request,
                         CategoryResponse *response) override {
    std::cout << "GetCategoryById called" << std::endl;

    if (!request->has_category() || request->category().id().empty()) {
      return Status(grpc::INVALID_ARGUMENT, "Category ID is required");
    }

    auto category = storage_.GetCategoryById(request->category().id());
    if (category) {
      *response->mutable_category() = *category;
      return Status::OK;
    }

    return Status(grpc::NOT_FOUND, "Category not found");
  }
};

void RunServer(toml::table &cfg) {

  // Конфигурация
  const std::string address{
      cfg["server_parameters"]["host"].value_or("0.0.0.0")};
  const unsigned short port{static_cast<unsigned short>(
      cfg["server_parameters"]["port"].value_or(15000))};

  std::string server_address(std::format("{}:{}", address, port));
  EcommerceServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  //   auto logger{Logging::LoggerFactory::GetLogger(
  //       cfg["logging"]["filename"].value_or("grpc_server.log"))};

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