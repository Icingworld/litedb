#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace litedb::core::schema
{

// 默认值表达式类型
enum class DefaultExpressionKind : std::uint8_t
{
    Literal = 0,
    Vector = 1,
};

// 默认值字面量类型
enum class DefaultLiteralKind : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    Float = 3,
    String = 4,
};

// 可持久化的默认值表达式
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
