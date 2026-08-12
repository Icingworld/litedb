#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::executor
{

// 执行错误码
enum class ExecutionErrorCode : std::uint8_t
{
    UnsupportedStatement,
    InvalidPlan,
    CatalogError,
    SchemaError,
    StorageError,
    IndexError,
    TransactionError,
    EvaluationError,
    CollectionNotFound,
};

using ExecutionError = error::Error;

} // namespace litedb::core::executor

namespace litedb::core::error
{

template <>
struct ErrorTraits<executor::ExecutionErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Execution;
};

} // namespace litedb::core::error
