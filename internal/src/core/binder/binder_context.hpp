#pragma once

#include "core/binder/session_context.hpp"
#include "core/function/function_catalog.hpp"
#include "core/meta/meta_engine.hpp"

namespace litedb::core::binder
{

/**
 * @brief 绑定上下文
 */
class BinderContext
{
public:
    BinderContext(
        meta::CatalogView meta,
        const SessionContext & session,
        const function::FunctionCatalog & functions
    ) noexcept;

public:
    /**
     * @brief 获取数据库读取器
     * @return 数据库读取器
     */
    [[nodiscard]]
    const meta::CatalogView & meta() const noexcept;

    /**
     * @brief 获取会话上下文
     * @return 会话上下文
     */
    [[nodiscard]]
    const SessionContext & session() const noexcept;

    [[nodiscard]]
    const function::FunctionCatalog & functions() const noexcept;

private:
    meta::CatalogView meta_;              // 元数据视图
    const SessionContext & session_;            // 会话上下文
    const function::FunctionCatalog & functions_; // 函数目录
};

} // namespace litedb::core::binder
