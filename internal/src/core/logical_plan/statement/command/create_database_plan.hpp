#pragma once

#include <string>

#include "core/logical_plan/statement/logical_statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief CREATE DATABASE 语句计划
 */
class CreateDatabasePlan final : public LogicalStatementPlan
{
public:
    CreateDatabasePlan(std::string database_name, bool if_not_exists, parser::ast::AstNodeLocation location);

public:
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
    bool if_not_exists() const noexcept;

private:
    std::string database_name_;     ///< 数据库名称
    bool if_not_exists_;            ///< 是否存在
};

} // namespace litedb::core::planner::plan
