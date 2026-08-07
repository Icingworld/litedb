#pragma once

#include <cstdint>

namespace litedb::core::common
{

/**
 * @brief 一元运算符
 */
enum class UnaryOperator : std::uint8_t
{
    Negate,           // 取负
    Not,              // 取反
};

/**
 * @brief 二元运算符
 */
enum class BinaryOperator : std::uint8_t
{
    Add,              // 加法
    Subtract,         // 减法
    Multiply,         // 乘法
    Divide,           // 除法
    Modulus,          // 取模
    Power,            // 幂运算
    Equal,            // 等于
    NotEqual,         // 不等于
    LessThan,         // 小于
    LessThanOrEqual,  // 小于等于
    GreaterThan,      // 大于
    GreaterThanOrEqual, // 大于等于
    And,              // 与
    Or,               // 或
    Xor,              // 异或
    Like,             // 模糊匹配
};

} // namespace litedb::core::common
