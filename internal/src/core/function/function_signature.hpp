#pragma once

#include <string>
#include <vector>

#include "core/common/logical_type.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数类型
 */
enum class FunctionKind
{
    Scalar,                     ///< 标量函数
    Aggregate,                  ///< 聚合函数
};

/**
 * @brief 函数签名
 */
struct FunctionSignature
{
    std::string name;                                   ///< 函数名称
    std::vector<common::LogicalType> argument_types;    ///< 参数类型
    common::LogicalType return_type;                    ///< 返回类型
    bool variadic {false};                              ///< 是否可变参数
};

} // namespace litedb::core::function
