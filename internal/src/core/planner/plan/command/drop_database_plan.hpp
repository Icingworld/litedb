#pragma once

#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief DROP DATABASE ????
 */
class DropDatabasePlan final : public StatementPlan
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
     * @brief ????? ID
     * @return ??? ID
     */
    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

    /**
     * @brief ???????
     * @return ?????
     */
    [[nodiscard]]
    const std::string & database_name() const noexcept;

    /**
     * @brief ????
     * @return ????
     */
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::optional<common::DatabaseId> database_id_;     ///< ??? ID
    std::string database_name_;                         ///< ?????
    bool if_exists_;                                    ///< ????
};

} // namespace litedb::core::planner::plan
