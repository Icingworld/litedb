#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/common/logical_id.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::executor
{

/**
 * @brief 执行结果类型
 */
enum class ExecutionResultKind
{
    Command,        ///< 命令结果
    RowSet,         ///< 行集结果
    UseDatabase,    ///< 切换数据库结果
};

/**
 * @brief 执行结果列
 */
struct ExecutionColumn
{
    std::string name;               ///< 列名
    common::LogicalType type;       ///< 列类型
};

/**
 * @brief 执行结果行
 */
struct ExecutionRow
{
    std::vector<schema::Value> values;      ///< 值列表
};

/**
 * @brief 执行结果
 */
struct ExecutionResult
{
    ExecutionResultKind kind {ExecutionResultKind::Command};     ///< 结果类型
    std::size_t affected_rows {0};                               ///< 影响行数
    std::vector<ExecutionColumn> columns;                        ///< 结果列
    std::vector<ExecutionRow> rows;                              ///< 结果行
    std::optional<common::DatabaseId> selected_database_id;      ///< 切换后的数据库 ID
    std::optional<std::string> selected_database_name;           ///< 切换后的数据库名称
};

} // namespace litedb::core::executor
