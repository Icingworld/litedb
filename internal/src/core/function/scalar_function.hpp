#pragma once

#include <expected>
#include <vector>

#include "core/function/function.hpp"
#include "core/function/function_error.hpp"
#include "core/common/value.hpp"

namespace litedb::core::function
{

/**
 * @brief 标量函数上下文
 */
struct ScalarFunctionContext
{
};

/**
 * @brief 标量函数
 */
class ScalarFunction final : public Function
{
public:
    /**
     * @brief 评估函数
     */
    using EvalFn = std::expected<common::Value, FunctionError> (*)(
        const std::vector<common::Value> & arguments,
        const ScalarFunctionContext & context,
        parser::ast::AstNodeLocation location
    );

public:
    ScalarFunction(
        std::string name,
        std::vector<FunctionSignature> signatures,
        EvalFn eval
    );

public:
    /**
     * @brief 获取函数签名
     * @return 函数签名
     */
    [[nodiscard]]
    const std::vector<FunctionSignature> & signatures() const noexcept override;

    /**
     * @brief 评估函数
     * @param arguments 参数
     * @param context 上下文
     * @param location 位置
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<common::Value, FunctionError> evaluate(
        const std::vector<common::Value> & arguments,
        const ScalarFunctionContext & context,
        parser::ast::AstNodeLocation location
    ) const;

private:
    std::vector<FunctionSignature> signatures_;     ///< 函数签名
    EvalFn eval_;                                   ///< 评估函数
};

} // namespace litedb::core::function
