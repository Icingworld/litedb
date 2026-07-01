#pragma once

#include <string>

#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief CREATE DATABASE ????
 */
class CreateDatabasePlan final : public StatementPlan
{
public:
    CreateDatabasePlan(std::string database_name, bool if_not_exists, parser::ast::AstNodeLocation location);

public:
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
    bool if_not_exists() const noexcept;

private:
    std::string database_name_;     ///< ?????
    bool if_not_exists_;            ///< ????
};

} // namespace litedb::core::planner::plan
