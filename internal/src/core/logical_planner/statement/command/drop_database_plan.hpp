#pragma once

#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/logical_planner/statement/logical_statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief DROP DATABASE 语句计划
 */
class DropDatabasePlan final : public LogicalStatementPlan
{
public:
    DropDatabasePlan(
        std::optional<common::DatabaseId> database_id,
        std::string database_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    [[nodiscard]]
    const std::string & database_name() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::optional<common::DatabaseId> database_id_;     ///< 数据库 ID
    std::string database_name_;                         ///< 数据库名称
    bool if_exists_;                                    ///< 是否存在
};

} // namespace litedb::core::planner::plan
