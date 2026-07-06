#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/function/function_signature.hpp"
#include "core/function/scalar_function.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定函数表达式节点
 * @details 示例：function_name(arguments)
 */
class BoundFunctionExpression final : public BoundExpression
{
public:
    BoundFunctionExpression(
        std::string name,
        std::shared_ptr<const function::ScalarFunction> function,
        function::FunctionSignature signature,
        std::vector<std::unique_ptr<BoundExpression>> arguments,
        common::LogicalType type,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取函数名称
     * @return 函数名称
     */
    [[nodiscard]]
    const std::string & name() const noexcept;

    /**
     * @brief 获取函数
     * @return 函数
     */
    [[nodiscard]]
    const function::ScalarFunction & function() const noexcept;

    /**
     * @brief 获取函数签名
     * @return 函数签名
     */
    [[nodiscard]]
    const function::FunctionSignature & signature() const noexcept;

    /**
     * @brief 获取参数列表
     * @return 参数列表
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & arguments() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundExpressionVisitor & visitor) const override;

    /**
     * @brief 深拷贝表达式
     * @return 表达式副本
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> clone() const override;

private:
    std::string name_;                                          ///< 函数名称
    std::shared_ptr<const function::ScalarFunction> function_;  ///< 函数
    function::FunctionSignature signature_;                     ///< 函数签名
    std::vector<std::unique_ptr<BoundExpression>> arguments_;   ///< 参数列表
};

} // namespace litedb::core::binder::bound
