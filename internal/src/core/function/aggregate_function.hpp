#pragma once

#include <expected>
#include <memory>
#include <vector>

#include "core/function/function.hpp"
#include "core/function/function_error.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::function
{

/**
 * @brief 聚合状态
 */
class AggregateState
{
public:
    virtual ~AggregateState() noexcept = default;
};

/**
 * @brief 聚合函数
 */
class AggregateFunction : public Function
{
public:
    explicit AggregateFunction(std::string name);

public:
    /**
     * @brief 创建聚合状态
     * @return 聚合状态
     */
    [[nodiscard]]
    virtual std::unique_ptr<AggregateState> create_state() const = 0;

    /**
     * @brief 更新聚合状态
     * @param state 聚合状态
     * @param arguments 参数
     * @param location 位置
     * @return 更新结果
     */
    [[nodiscard]]
    virtual std::expected<void, FunctionError> update(
        AggregateState & state,
        const std::vector<schema::Value> & arguments,
        parser::ast::AstNodeLocation location
    ) const = 0;

    /**
     * @brief 最终化聚合状态
     * @param state 聚合状态
     * @param location 位置
     * @return 最终化结果
     */
    [[nodiscard]]
    virtual std::expected<schema::Value, FunctionError> finalize(
        const AggregateState & state,
        parser::ast::AstNodeLocation location
    ) const = 0;
};

} // namespace litedb::core::function
