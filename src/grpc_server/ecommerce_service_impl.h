#pragma once

#include "logger.h"

#include "in_memory_storage.h"

#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using ecommerce::AttachOrDetachRequest;
using ecommerce::CategoryListResponse;
using ecommerce::CategoryRequest;
using ecommerce::CategoryResponse;
using ecommerce::Ecommerce;
using ecommerce::ProductListResponse;
using ecommerce::ProductRequest;
using ecommerce::ProductResponse;

class EcommerceServiceImpl final : public Ecommerce::Service {

public:
  EcommerceServiceImpl(Logging::Logger &logger);

  Status AddProduct(ServerContext *context, const ProductRequest *request,
                    ProductResponse *response) override;

  Status EditProduct(ServerContext *context, const ProductRequest *request,
                     ProductResponse *response) override;

  Status DeleteProduct(ServerContext *context, const ProductRequest *request,
                       ProductResponse *response) override;

  Status AddCategory(ServerContext *context, const CategoryRequest *request,
                     CategoryResponse *response) override;

  Status EditCategory(ServerContext *context, const CategoryRequest *request,
                      CategoryResponse *response) override;

  Status DeleteCategory(ServerContext *context, const CategoryRequest *request,
                        CategoryResponse *response) override;

  Status AttachProductToCategory(ServerContext *context,
                                 const AttachOrDetachRequest *request,
                                 CategoryResponse *response) override;

  Status DetachProductFromCategory(ServerContext *context,
                                   const AttachOrDetachRequest *request,
                                   CategoryResponse *response) override;

  Status GetAllProducts(ServerContext *context, const ProductRequest *request,
                        ProductListResponse *response) override;

  Status GetAllCategories(ServerContext *context,
                          const CategoryRequest *request,
                          CategoryListResponse *response) override;

  Status GetProductById(ServerContext *context, const ProductRequest *request,
                        ProductResponse *response) override;

  Status GetCategoryById(ServerContext *context, const CategoryRequest *request,
                         CategoryResponse *response) override;

private:
  Logging::Logger &logger_;

  InMemoryStorage storage_;
};