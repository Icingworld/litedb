#pragma once

#include "core/binder/session_context.hpp"
#include "core/catalog/catalog_reader.hpp"

namespace litedb::core::binder
{

class BinderContext
{
public:
    BinderContext(const catalog::CatalogReader & catalog, const SessionContext & session) noexcept;

public:
    [[nodiscard]]
    const catalog::CatalogReader & catalog() const noexcept;

    [[nodiscard]]
    const SessionContext & session() const noexcept;

private:
    const catalog::CatalogReader & catalog_;    ///< 数据库读取器
    const SessionContext & session_;            ///< 会话上下文
};

} // namespace litedb::core::binder
