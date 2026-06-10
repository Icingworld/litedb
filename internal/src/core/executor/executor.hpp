#pragma once

#include <expected>

#include "core/catalog/catalog.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/planner/statement/statement_plan.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::executor
{

/**
 * @brief 执行器
 */
class Executor
{
public:
    Executor(catalog::Catalog & catalog, storage::StorageManager & storage) noexcept;

public:
    /**
     * @brief 执行语句计划
     * @param plan 语句计划
     * @return 执行结果
     */
    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(const planner::StatementPlan & plan);

private:
    catalog::Catalog & catalog_;               ///< 目录
    storage::StorageManager & storage_;        ///< 存储管理器
};

} // namespace litedb::core::executor
