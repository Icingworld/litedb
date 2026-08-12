#pragma once

#include "core/catalog/catalog_viewer.hpp"

namespace litedb::core::physical_planner
{

// Physical Planner worker 共享的只读上下文
class PhysicalPlannerContext final
{
public:
    explicit PhysicalPlannerContext(catalog::CatalogViewer catalog) noexcept;

public:
    // 获取目录视图
    [[nodiscard]]
    const catalog::CatalogViewer & catalog() const noexcept;

private:
    catalog::CatalogViewer catalog_;
};

} // namespace litedb::core::physical_planner
