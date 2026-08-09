#pragma once

#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/meta/entry/index_entry.hpp"

namespace litedb::core::logical_planner::plan
{

// CREATE INDEX 语句逻辑计划
class CreateIndexPlan final : public LogicalPlan
{
public:
    CreateIndexPlan(
        common::ColumnId column_id,
        std::optional<std::string> index_name,
        meta::entry::IndexKind index_kind,
        bool unique
    );

public:
    // 获取列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 获取索引名称
    [[nodiscard]]
    const std::optional<std::string> & index_name() const noexcept;

    // 获取索引类型
    [[nodiscard]]
    meta::entry::IndexKind index_kind() const noexcept;

    // 是否唯一
    [[nodiscard]]
    bool unique() const noexcept;

private:
    common::ColumnId column_id_;
    std::optional<std::string> index_name_;
    meta::entry::IndexKind index_kind_;
    bool unique_;
};

} // namespace litedb::core::logical_planner::plan
