#pragma once

#include <expected>
#include <string>

#include "core/binder/binder_error.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::meta::entry
{

class CollectionEntry;

} // namespace litedb::core::meta::entry

namespace litedb::core::binder
{

class BinderContext;

} // namespace litedb::core::binder

namespace litedb::core::binder::detail
{

// 绑定集合
struct BindingCollection
{
    common::DatabaseId database_id {0};
    const meta::entry::CollectionEntry * collection {nullptr};
};

// Catalog 解析器
class CatalogResolver
{
public:
    explicit CatalogResolver(const BinderContext & context);

public:
    // 获取当前数据库
    [[nodiscard]]
    std::expected<common::DatabaseId, BinderError> require_database() const;

    // 解析集合
    [[nodiscard]]
    std::expected<BindingCollection, BinderError> resolve_collection(
        const std::string & collection_name
    ) const;

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder::detail
