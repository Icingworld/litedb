#pragma once

#include "core/common/ids.hpp"
#include "core/logical_plan/statement/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief SHOW COLLECTIONS 语句计划
 */
class ShowCollectionsPlan final : public StatementPlan
{
public:
    ShowCollectionsPlan(common::DatabaseId database_id, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;             ///< 数据库 ID
};

} // namespace litedb::core::planner::plan
