#pragma once

#include <span>

#include "core/common/value.hpp"
#include "core/function/scalar_function.hpp"

namespace litedb::core::evaluator
{

/**
 * @brief 评估器上下文
 */
struct EvaluationContext
{
    std::span<const common::Value> input_values {};           // 输入值
    function::ScalarFunctionContext function_context {};      // 函数上下文
};

} // namespace litedb::core::evaluator
