#pragma once

#include "core/meta/meta_engine.hpp"

namespace litedb::core::physical_planner
{

// Physical Planner worker 共享的只读上下文
class PhysicalPlannerContext final
{
public:
    explicit PhysicalPlannerContext(meta::CatalogView catalog) noexcept;

public:
    // 获取目录视图
    [[nodiscard]]
    const meta::CatalogView & catalog() const noexcept;

private:
    meta::CatalogView catalog_;
};

} // namespace litedb::core::physical_planner
