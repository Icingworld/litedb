#pragma once

#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定常量表达式
 */
class BoundLiteralExpression final : public BoundExpression
{
public:
    BoundLiteralExpression(
        common::LogicalType type,
        std::string value
    );

public:
    /**
     * @brief 获取常量值
     * @return 常量值
     */
    [[nodiscard]]
    const std::string & value() const noexcept;

private:
    std::string value_;     ///< 常量值
};

} // namespace litedb::core::binder::bound
