#pragma once

#include <cstddef>
#include <expected>
#include <memory>

#include "core/optimizer/optimizer_error.hpp"
#include "core/logical_plan/statement/logical_statement_plan.hpp"

namespace litedb::core::meta
{

class MetaEngine;

} // namespace litedb::core::meta

namespace litedb::core::optimizer
{

/**
 * @brief 优化器选项
 */
struct OptimizerOptions
{
    bool enabled {true};                         ///< 是否启用优化器
    bool enable_constant_folding {true};         ///< 是否启用常量折叠
    bool enable_boolean_simplification {true};   ///< 是否启用布尔简化
    bool enable_filter_elimination {true};       ///< 是否启用 Filter(true) 消除
    bool enable_index_selection {true};          ///< 是否启用简单索引选择
    std::size_t max_passes {8};                  ///< 固定点迭代上限
};

/**
 * @brief 逻辑计划优化器
 */
class Optimizer
{
public:
    explicit Optimizer(OptimizerOptions options = {}, const meta::MetaEngine * catalog = nullptr) noexcept;

    /**
     * @brief 优化 statement plan
     * @param plan statement plan
     * @return 优化后的 statement plan
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<planner::plan::LogicalStatementPlan>, OptimizerError> optimize(
        std::unique_ptr<planner::plan::LogicalStatementPlan> plan
    ) const;

private:
    OptimizerOptions options_;
    const meta::MetaEngine * catalog_ {nullptr};
};

} // namespace litedb::core::optimizer
