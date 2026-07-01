#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief SHOW INDEXES 语句计划
 */
class ShowIndexesPlan final : public StatementPlan
{
public:
    ShowIndexesPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
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
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    common::DatabaseId database_id_;            ///< 数据库 ID
    common::CollectionId collection_id_;        ///< 集合 ID
    std::string collection_name_;               ///< 集合名称
};

} // namespace litedb::core::planner::plan
