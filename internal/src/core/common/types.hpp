#pragma once

#include <cstdint>

namespace litedb::core::common
{

/**
 * @brief 一元运算符
 */
enum class UnaryOperator : std::uint8_t
{
    Negate,           ///< 取负
    Not,              ///< 取反
};

/**
 * @brief 二元运算符
 */
enum class BinaryOperator : std::uint8_t
{
    Add,              ///< 加法
    Subtract,         ///< 减法
    Multiply,         ///< 乘法
    Divide,           ///< 除法
    Modulus,          ///< 取模
    Power,            ///< 幂运算
};

} // namespace litedb::core::common
