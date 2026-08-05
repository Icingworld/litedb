#pragma once

#include <cstdint>

namespace litedb::core::physical_planner::plan
{

enum class PhysicalPlanKind : std::uint8_t
{
    Use = 0,
    CreateDatabase,
    CreateCollection,
    CreateIndex,
    CreateVectorIndex,
    DropDatabase,
    DropCollection,
    DropIndex,
    DropVectorIndex,
    ShowDatabases,
    ShowCollections,
    ShowIndexes,
    ShowVectorIndexes,
    DescribeCollection,
    Insert,
    Update,
    Delete,
    Query,
};

class PhysicalPlan
{
public:
    PhysicalPlan(const PhysicalPlan &) = delete;
    PhysicalPlan & operator=(const PhysicalPlan &) = delete;

    PhysicalPlan(PhysicalPlan &&) noexcept = default;
    PhysicalPlan & operator=(PhysicalPlan &&) noexcept = default;

    virtual ~PhysicalPlan() noexcept = default;

protected:
    explicit PhysicalPlan(PhysicalPlanKind kind) noexcept
        : kind_(kind)
    {
    }

public:
    [[nodiscard]] PhysicalPlanKind kind() const noexcept
    {
        return kind_;
    }

private:
    PhysicalPlanKind kind_;
};

} // namespace litedb::core::physical_planner::plan
