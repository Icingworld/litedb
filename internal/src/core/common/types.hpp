#pragma once

#include <cstdint>

namespace litedb::core::common
{

// 一元运算符
enum class UnaryOperator : std::uint8_t
{
    Negate, // 取负
    Not, // 取反
};

// 二元运算符
enum class BinaryOperator : std::uint8_t
{
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulus,
    Power,
    Equal,
    NotEqual,
    LessThan,
    LessThanOrEqual,
    GreaterThan,
    GreaterThanOrEqual,
    And,
    Or,
    Xor,
    Like,
};

} // namespace litedb::core::common
