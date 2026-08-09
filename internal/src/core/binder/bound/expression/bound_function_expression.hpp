#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/function/scalar_function.hpp"

namespace litedb::core::binder::bound
{

// 绑定函数表达式
class BoundFunctionExpression final : public BoundExpression
{
public:
    BoundFunctionExpression(
        function::BoundScalarFunction function,
        std::vector<std::unique_ptr<BoundExpression>> arguments
    );

public:
    // 获取函数
    [[nodiscard]]
    const function::BoundScalarFunction & function() const noexcept;

    // 获取参数列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & arguments() const noexcept;

    // 获取参数所有权
    [[nodiscard]]
    std::vector<std::unique_ptr<BoundExpression>> take_arguments() noexcept;

private:
    function::BoundScalarFunction function_;
    std::vector<std::unique_ptr<BoundExpression>> arguments_;
};

} // namespace litedb::core::binder::bound
