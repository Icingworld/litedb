#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/planner/statement/statement_plan.hpp"

namespace litedb::core::planner
{

/**
 * @brief USE 语句计划
 */
class UsePlan final : public StatementPlan
{
public:
    UsePlan(common::DatabaseId database_id, std::string database_name, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    [[nodiscard]]
    const std::string & database_name() const noexcept;

private:
    common::DatabaseId database_id_;             ///< 数据库ID
    std::string database_name_;                  ///< 数据库名称
};

} // namespace litedb::core::planner
