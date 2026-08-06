#pragma once

#include <optional>
#include <string>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/index_entry.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief CREATE INDEX 语句计划
 */
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
    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief 获取索引名称
     * @return 索引名称
     */
    [[nodiscard]]
    const std::optional<std::string> & index_name() const noexcept;

    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    meta::entry::IndexKind index_kind() const noexcept;

    /**
     * @brief 是否唯一
     * @return 是否唯一
     */
    [[nodiscard]]
    bool unique() const noexcept;

private:
    common::ColumnId column_id_;                ///< 列 ID
    std::optional<std::string> index_name_;     ///< 索引名称
    meta::entry::IndexKind index_kind_;         ///< 索引类型
    bool unique_;                               ///< 是否唯一
};

} // namespace litedb::core::logical_planner::plan
