#pragma once

#include "core/binder/session_context.hpp"
#include "core/function/function_catalog.hpp"
#include "core/catalog/catalog_viewer.hpp"

namespace litedb::core::binder
{

// 绑定器上下文
class BinderContext
{
public:
    BinderContext(
        catalog::CatalogViewer catalog,
        const SessionContext & session,
        const function::FunctionCatalog & functions
    ) noexcept;

public:
    // 获取数据库读取器
    [[nodiscard]]
    const catalog::CatalogViewer & catalog() const noexcept;

    // 获取会话上下文
    [[nodiscard]]
    const SessionContext & session() const noexcept;

    [[nodiscard]]
    const function::FunctionCatalog & functions() const noexcept;

private:
    catalog::CatalogViewer catalog_;
    const SessionContext & session_;
    const function::FunctionCatalog & functions_;
};

} // namespace litedb::core::binder
