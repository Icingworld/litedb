#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/common/ids.hpp"
#include "core/meta/meta_request.hpp"

namespace litedb::core::logical_planner::plan
{

// CREATE COLLECTION 语句逻辑计划
class CreateCollectionPlan final : public LogicalPlan
{
public:
    CreateCollectionPlan(
        common::DatabaseId database_id,
        std::optional<std::string> collection_name,
        std::vector<meta::ColumnDefinition> columns,
        std::optional<std::string> comment
    );

public:
    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    const std::optional<std::string> & collection_name() const noexcept;

    // 获取列定义
    [[nodiscard]]
    const std::vector<meta::ColumnDefinition> & columns() const noexcept;

    // 获取列定义所有权
    [[nodiscard]]
    std::vector<meta::ColumnDefinition> take_columns() noexcept;

    // 获取集合注释
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::DatabaseId database_id_;
    std::optional<std::string> collection_name_;
    std::vector<meta::ColumnDefinition> columns_;
    std::optional<std::string> comment_;
};

} // namespace litedb::core::logical_planner::plan
