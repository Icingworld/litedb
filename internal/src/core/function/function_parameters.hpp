#pragma once

#include <optional>
#include <vector>

#include "core/common/logical_type.hpp"

namespace litedb::core::function
{

// 函数参数定义
// 函数参数包括固定参数和可变参数两部分
// 对一个函数来说，固定参数在前，可变参数在后
// 且可变参数虽然数量不固定，但类型必须是相同的
// 例如：
// - 函数 cal(x: double, y: double, ...) -> double
//   其中 x 和 y 是固定参数，... 是可变参数，比如可以传入三个 int 参数
struct FunctionParameters
{
    std::vector<common::LogicalType> fixed;
    std::optional<common::LogicalType> variadic; // 可变参数
};

} // namespace litedb::core::function
