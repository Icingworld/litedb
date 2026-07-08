#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace litedb::core::meta::entry
{

/**
 * @brief 默认值表达式类型
 */
enum class DefaultExpressionKind : std::uint8_t
{
    Literal,                ///< 字面量
    Vector,                 ///< 向量
};

/**
 * @brief 默认值字面量类型
 */
enum class DefaultLiteralKind : std::uint8_t
{
    Null,                   ///< 空
    Boolean,                ///< 布尔值
    Integer,                ///< 整数
    Float,                  ///< 浮点数
    String,                 ///< 字符串
};

/**
 * @brief 默认值表达式
 */
struct DefaultExpression
{
    DefaultExpressionKind kind {DefaultExpressionKind::Literal};       ///< 默认值表达式类型
    DefaultLiteralKind literal_kind {DefaultLiteralKind::Null};        ///< 默认值字面量类型
    std::string value;                                                 ///< 默认值字面量值
    std::vector<DefaultExpression> elements;                           ///< 默认值向量元素

    /**
     * @brief 创建空字面量
     * @return 空字面量
     */
    [[nodiscard]]
    static DefaultExpression null_literal();

    /**
     * @brief 创建字面量
     * @param literal_kind 字面量类型
     * @param value 字面量值
     * @return 字面量
     */
    [[nodiscard]]
    static DefaultExpression literal(DefaultLiteralKind literal_kind, std::string value);

    /**
     * @brief 创建向量
     * @param elements 向量元素
     * @return 向量
     */
    [[nodiscard]]
    static DefaultExpression vector(std::vector<DefaultExpression> elements);
};

} // namespace litedb::core::meta::entry
