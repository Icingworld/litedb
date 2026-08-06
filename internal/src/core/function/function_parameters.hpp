#pragma once

#include <optional>
#include <vector>

#include "core/common/logical_type.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数参数定义
 */
struct FunctionParameters
{
    std::vector<common::LogicalType> fixed;           ///< 固定参数
    std::optional<common::LogicalType> variadic;      ///< 可变参数
};

} // namespace litedb::core::function
