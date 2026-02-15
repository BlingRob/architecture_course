#include "ecommerce_service_impl.h"

EcommerceServiceImpl::EcommerceServiceImpl(Logging::Logger &logger)
    : logger_(logger) {}

Status EcommerceServiceImpl::AddProduct(ServerContext *context,
                                        const ProductRequest *request,
                                        ProductResponse *response) {

  LOG_DEBUG(logger_.get(), "AddProduct called");

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

Status EcommerceServiceImpl::EditProduct(ServerContext *context,
                                         const ProductRequest *request,
                                         ProductResponse *response) {
  LOG_DEBUG(logger_.get(), "EditProduct called");

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

Status EcommerceServiceImpl::DeleteProduct(ServerContext *context,
                                           const ProductRequest *request,
                                           ProductResponse *response) {
  LOG_DEBUG(logger_.get(), "DeleteProduct called");

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

Status EcommerceServiceImpl::AddCategory(ServerContext *context,
                                         const CategoryRequest *request,
                                         CategoryResponse *response) {
  LOG_DEBUG(logger_.get(), "AddCategory called");

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

Status EcommerceServiceImpl::EditCategory(ServerContext *context,
                                          const CategoryRequest *request,
                                          CategoryResponse *response) {
  LOG_DEBUG(logger_.get(), "EditCategory called");

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

Status EcommerceServiceImpl::DeleteCategory(ServerContext *context,
                                            const CategoryRequest *request,
                                            CategoryResponse *response) {
  LOG_DEBUG(logger_.get(), "DeleteCategory called");

  if (!request->has_category() || request->category().id().empty()) {
    return Status(grpc::INVALID_ARGUMENT, "Category ID is required");
  }

  if (storage_.DeleteCategory(request->category().id())) {
    response->mutable_category()->set_id(request->category().id());
    return Status::OK;
  }

  return Status(grpc::NOT_FOUND, "Category not found");
}

Status EcommerceServiceImpl::AttachProductToCategory(
    ServerContext *context, const AttachOrDetachRequest *request,
    CategoryResponse *response) {
  LOG_DEBUG(logger_.get(), "AttachProductToCategory called");

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

Status EcommerceServiceImpl::DetachProductFromCategory(
    ServerContext *context, const AttachOrDetachRequest *request,
    CategoryResponse *response) {
  LOG_DEBUG(logger_.get(), "DetachProductFromCategory called");

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

Status EcommerceServiceImpl::GetAllProducts(ServerContext *context,
                                            const ProductRequest *request,
                                            ProductListResponse *response) {
  LOG_DEBUG(logger_.get(), "GetAllProducts called");

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

Status EcommerceServiceImpl::GetAllCategories(ServerContext *context,
                                              const CategoryRequest *request,
                                              CategoryListResponse *response) {
  LOG_DEBUG(logger_.get(), "GetAllCategories called");

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

Status EcommerceServiceImpl::GetProductById(ServerContext *context,
                                            const ProductRequest *request,
                                            ProductResponse *response) {
  LOG_DEBUG(logger_.get(), "GetProductById called");

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

Status EcommerceServiceImpl::GetCategoryById(ServerContext *context,
                                             const CategoryRequest *request,
                                             CategoryResponse *response) {
  LOG_DEBUG(logger_.get(), "GetCategoryById called");

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