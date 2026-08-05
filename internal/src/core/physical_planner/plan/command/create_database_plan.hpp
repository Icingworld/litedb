#pragma once

#include <optional>
#include <string>

#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

/**
 * @brief CREATE DATABASE 语句计划
 */
class CreateDatabasePlan final : public PhysicalPlan
{
public:
    explicit CreateDatabasePlan(std::optional<std::string> database_name);

public:
    /**
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    [[nodiscard]]
    const std::optional<std::string> & database_name() const noexcept;

private:
    std::optional<std::string> database_name_;     ///< 数据库名称
};

} // namespace litedb::core::physical_planner::plan