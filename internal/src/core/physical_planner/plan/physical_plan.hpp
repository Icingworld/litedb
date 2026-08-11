#pragma once

#include <cstdint>

namespace litedb::core::physical_planner::plan
{

// 物理计划类型
enum class PhysicalPlanKind : std::uint8_t
{
    Use,
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

// 物理计划
class PhysicalPlan
{
public:
    PhysicalPlan(const PhysicalPlan &) = delete;

    PhysicalPlan & operator=(const PhysicalPlan &) = delete;

    PhysicalPlan(PhysicalPlan &&) noexcept = default;

    PhysicalPlan & operator=(PhysicalPlan &&) noexcept = default;

    virtual ~PhysicalPlan() noexcept = default;

protected:
    explicit PhysicalPlan(PhysicalPlanKind kind) noexcept;

public:
    // 获取物理计划类型
    [[nodiscard]]
    PhysicalPlanKind kind() const noexcept;

private:
    PhysicalPlanKind kind_;
};

} // namespace litedb::core::physical_planner::plan
