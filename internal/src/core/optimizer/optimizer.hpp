#pragma once

#include <cstddef>
#include <expected>
#include <memory>

#include "core/optimizer/optimizer_error.hpp"
#include "core/logical_plan/statement/statement_plan.hpp"

namespace litedb::core::catalog
{

class CatalogReader;

} // namespace litedb::core::catalog

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
    explicit Optimizer(OptimizerOptions options = {}, const catalog::CatalogReader * catalog = nullptr) noexcept;

    /**
     * @brief 优化 statement plan
     * @param plan statement plan
     * @return 优化后的 statement plan
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<planner::plan::StatementPlan>, OptimizerError> optimize(
        std::unique_ptr<planner::plan::StatementPlan> plan
    ) const;

private:
    OptimizerOptions options_;
    const catalog::CatalogReader * catalog_ {nullptr};
};

} // namespace litedb::core::optimizer
