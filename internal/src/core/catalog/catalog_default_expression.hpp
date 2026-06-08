#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace litedb::core::catalog
{

/**
 * @brief 默认值表达式类型
 */
enum class CatalogDefaultExpressionKind : std::uint8_t
{
    Literal,
    Vector,
};    

/**
 * @brief 默认值字面量类型
 */   
enum class CatalogDefaultLiteralKind : std::uint8_t
{
    Null,
    Boolean,
    Integer,
    Float,
    String,
};

/**
 * @brief 默认值表达式
 */
struct CatalogDefaultExpression
{
    CatalogDefaultExpressionKind kind {CatalogDefaultExpressionKind::Literal};  ///< 默认值表达式类型
    CatalogDefaultLiteralKind literal_kind {CatalogDefaultLiteralKind::Null};   ///< 默认值字面量类型
    std::string value;                                                          ///< 默认值字面量值
    std::vector<CatalogDefaultExpression> elements;                             ///< 默认值向量元素

    /**
     * @brief 创建空字面量
     * @return 空字面量
     */
    [[nodiscard]]
    static CatalogDefaultExpression null_literal();

    /**
     * @brief 创建字面量
     * @param literal_kind 字面量类型
     * @param value 字面量值
     * @return 字面量
     */
    [[nodiscard]]
    static CatalogDefaultExpression literal(CatalogDefaultLiteralKind literal_kind, std::string value);

    /**
     * @brief 创建向量
     * @param elements 向量元素
     * @return 向量
     */
    [[nodiscard]]
    static CatalogDefaultExpression vector(std::vector<CatalogDefaultExpression> elements);
};

} // namespace litedb::core::catalog
