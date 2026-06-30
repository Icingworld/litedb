#pragma once

#include "core/binder/session_context.hpp"
#include "core/catalog/catalog_reader.hpp"

namespace litedb::core::binder
{

/**
 * @brief 绑定上下文
 */
class BinderContext
{
public:
    BinderContext(const catalog::CatalogReader & catalog, const SessionContext & session) noexcept;

public:
    /**
     * @brief 获取数据库读取器
     * @return 数据库读取器
     */
    [[nodiscard]]
    const catalog::CatalogReader & catalog() const noexcept;

    /**
     * @brief 获取会话上下文
     * @return 会话上下文
     */
    [[nodiscard]]
    const SessionContext & session() const noexcept;

private:
    const catalog::CatalogReader & catalog_;    ///< 数据库读取器
    const SessionContext & session_;            ///< 会话上下文
};

} // namespace litedb::core::binder
