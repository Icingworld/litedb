#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/catalog/catalog_writer.hpp"
#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief CREATE COLLECTION 语句计划
 */
class CreateCollectionPlan final : public StatementPlan
{
public:
    CreateCollectionPlan(
        common::DatabaseId database_id,
        std::string collection_name,
        bool if_not_exists,
        std::vector<catalog::ColumnDefinition> columns,
        std::optional<std::string> comment,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    /**
     * @brief 获取列定义
     * @return 列定义
     */
    [[nodiscard]]
    const std::vector<catalog::ColumnDefinition> & columns() const noexcept;

    /**
     * @brief 获取集合注释
     * @return 集合注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::DatabaseId database_id_;                                ///< 数据库 ID
    std::string collection_name_;                                   ///< 集合名称
    bool if_not_exists_;                                            ///< 是否存在
    std::vector<catalog::ColumnDefinition> columns_;                ///< 列定义
    std::optional<std::string> comment_;                            ///< 集合注释
};

} // namespace litedb::core::planner::plan
