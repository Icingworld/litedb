#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief INSERT ????
 */
class InsertPlan final : public StatementPlan
{
public:
    InsertPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<binder::bound::BoundColumn> columns,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> values,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief ????? ID
     * @return ??? ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief ???? ID
     * @return ?? ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief ??????
     * @return ????
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief ???
     * @return ?
     */
    [[nodiscard]]
    const std::vector<binder::bound::BoundColumn> & columns() const noexcept;

    /**
     * @brief ???
     * @return ?
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & values() const noexcept;

private:
    common::DatabaseId database_id_;                                            ///< ??? ID
    common::CollectionId collection_id_;                                        ///< ?? ID
    std::string collection_name_;                                               ///< ????
    std::vector<binder::bound::BoundColumn> columns_;                           ///< ?
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;       ///< ?
};

} // namespace litedb::core::planner::plan
