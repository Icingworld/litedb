#include "core/logical_planner/plan/command/create_index_plan.hpp"

#include <utility>

namespace litedb::core::logical_planner::plan
{

CreateIndexPlan::CreateIndexPlan(
    common::ColumnId column_id,
    std::optional<std::string> index_name,
    meta::entry::IndexKind index_kind,
    bool unique
)
    : LogicalPlan(LogicalPlanKind::CreateIndex)
    , column_id_(column_id)
    , index_name_(std::move(index_name))
    , index_kind_(index_kind)
    , unique_(unique)
{}

common::ColumnId CreateIndexPlan::column_id() const noexcept
{
    return column_id_;
}

std::optional<const std::string &> CreateIndexPlan::index_name() const noexcept
{
    return index_name_;
}

std::optional<std::string> CreateIndexPlan::take_index_name() noexcept
{
    return std::exchange(index_name_, std::nullopt);
}

meta::entry::IndexKind CreateIndexPlan::index_kind() const noexcept
{
    return index_kind_;
}

bool CreateIndexPlan::unique() const noexcept
{
    return unique_;
}

} // namespace litedb::core::logical_planner::plan
