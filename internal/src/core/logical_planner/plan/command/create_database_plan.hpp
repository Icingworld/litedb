#pragma once

#include <optional>
#include <string>

#include "core/logical_planner/plan/logical_plan.hpp"

namespace litedb::core::logical_planner::plan
{

// CREATE DATABASE 语句逻辑计划
class CreateDatabasePlan final : public LogicalPlan
{
public:
    explicit CreateDatabasePlan(std::optional<std::string> database_name);

public:
    // 获取数据库名称
    [[nodiscard]]
    const std::optional<std::string> & database_name() const noexcept;

private:
    std::optional<std::string> database_name_;
};

} // namespace litedb::core::logical_planner::plan
