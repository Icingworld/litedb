#pragma once

#include <optional>
#include <string>

#include "core/physical_planner/plan/physical_plan.hpp"

namespace litedb::core::physical_planner::plan
{

// CREATE DATABASE 语句物理计划
class CreateDatabasePlan final : public PhysicalPlan
{
public:
    explicit CreateDatabasePlan(std::optional<std::string> database_name);

public:
    // 获取数据库名称
    [[nodiscard]]
    std::optional<const std::string &> database_name() const noexcept;

private:
    std::optional<std::string> database_name_;
};

} // namespace litedb::core::physical_planner::plan