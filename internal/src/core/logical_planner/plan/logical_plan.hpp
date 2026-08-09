#pragma once

#include <cstdint>

namespace litedb::core::logical_planner::plan
{

// 逻辑计划类型
enum class LogicalPlanKind : std::uint8_t
{
    // command
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

    // mutation
    Insert,
    Update,
    Delete,

    // query
    Query,
};

// 逻辑计划
class LogicalPlan
{
public:
    LogicalPlan(const LogicalPlan &) = delete;

    LogicalPlan & operator=(const LogicalPlan &) = delete;

    LogicalPlan(LogicalPlan &&) noexcept = default;

    LogicalPlan & operator=(LogicalPlan &&) noexcept = default;

    virtual ~LogicalPlan() noexcept = default;

protected:
    LogicalPlan(LogicalPlanKind kind) noexcept;

public:
    // 获取逻辑计划类型
    [[nodiscard]]
    LogicalPlanKind kind() const noexcept;

private:
    LogicalPlanKind kind_;
};

} // namespace litedb::core::logical_planner::plan
