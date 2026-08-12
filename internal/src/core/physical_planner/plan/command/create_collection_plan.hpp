#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/catalog/catalog_request.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// CREATE COLLECTION 语句物理计划
class CreateCollectionPlan final : public PhysicalPlan
{
public:
    CreateCollectionPlan(
        common::DatabaseId database_id,
        std::optional<std::string> collection_name,
        std::vector<catalog::ColumnDefinition> columns,
        std::optional<std::string> comment
    );

public:
    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    std::optional<const std::string &> collection_name() const noexcept;

    // 获取列定义
    [[nodiscard]]
    const std::vector<catalog::ColumnDefinition> & columns() const noexcept;

    // 获取集合注释
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::DatabaseId database_id_;
    std::optional<std::string> collection_name_;
    std::vector<catalog::ColumnDefinition> columns_;
    std::optional<std::string> comment_;
};

} // namespace litedb::core::physical_planner::plan