#pragma once

#include "core/meta/meta_engine.hpp"

namespace litedb::core::physical_planner
{

/**
 * @brief Physical Planner worker 共享的只读上下文
 */
class PhysicalPlannerContext final
{
public:
    explicit PhysicalPlannerContext(meta::CatalogView catalog) noexcept
        : catalog_(catalog)
    {
    }

public:
    [[nodiscard]]
    const meta::CatalogView & catalog() const noexcept
    {
        return catalog_;
    }

private:
    meta::CatalogView catalog_;
};

} // namespace litedb::core::physical_planner
