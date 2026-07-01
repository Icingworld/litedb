#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief DROP INDEX ????
 */
class DropIndexPlan final : public StatementPlan
{
public:
    DropIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::string index_name,
        bool if_exists,
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
     * @brief ??????
     * @return ????
     */
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    /**
     * @brief ????
     * @return ????
     */
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    common::DatabaseId database_id_;        ///< ??? ID
    common::CollectionId collection_id_;    ///< ?? ID
    std::string collection_name_;           ///< ????
    std::string index_name_;                ///< ????
    bool if_exists_;                        ///< ????
};

} // namespace litedb::core::planner::plan
