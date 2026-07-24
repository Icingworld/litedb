#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace litedb::core::schema
{

/**
 * @brief 默认值表达式类型
 */
enum class DefaultExpressionKind : std::uint8_t
{
    Literal = 0,
    Vector,
};

/**
 * @brief 默认值字面量类型
 */
enum class DefaultLiteralKind : std::uint8_t
{
    Null = 0,
    Boolean,
    Integer,
    Float,
    String,
};

/**
 * @brief 可持久化的默认值表达式
 */
struct DefaultExpression
{
    DefaultExpressionKind kind {DefaultExpressionKind::Literal};
    DefaultLiteralKind literal_kind {DefaultLiteralKind::Null};
    std::string value;
    std::vector<DefaultExpression> elements;

    [[nodiscard]]
    static DefaultExpression null_literal();

    [[nodiscard]]
    static DefaultExpression literal(DefaultLiteralKind literal_kind, std::string value);

    [[nodiscard]]
    static DefaultExpression vector(std::vector<DefaultExpression> elements);
};

} // namespace litedb::core::schema
