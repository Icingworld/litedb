#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/function/scalar_function.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定函数表达式
 */
class BoundFunctionExpression final : public BoundExpression
{
public:
    BoundFunctionExpression(
        function::BoundScalarFunction function,
        std::vector<std::unique_ptr<BoundExpression>> arguments
    );

public:
    /**
     * @brief 获取函数
     * @return 函数
     */
    [[nodiscard]]
    const function::BoundScalarFunction & function() const noexcept;

    /**
     * @brief 获取参数列表
     * @return 参数列表
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> &
    arguments() const noexcept;

    /**
     * @brief 移出函数参数
     * @return 参数所有权
     */
    [[nodiscard]]
    std::vector<std::unique_ptr<BoundExpression>> take_arguments() noexcept;

private:
    function::BoundScalarFunction function_;                    ///< 已绑定函数
    std::vector<std::unique_ptr<BoundExpression>> arguments_;   ///< 参数列表
};

} // namespace litedb::core::binder::bound
