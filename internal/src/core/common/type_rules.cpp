#include "core/common/type_rules.hpp"

namespace litedb::core::common
{

bool same_type(const LogicalType & left, const LogicalType & right) noexcept
{
    return left.id == right.id && left.parameter == right.parameter;
}

bool is_numeric(const LogicalType & type) noexcept
{
    return type.id == LogicalTypeId::Integer
        || type.id == LogicalTypeId::BigInt
        || type.id == LogicalTypeId::Float
        || type.id == LogicalTypeId::Double;
}

bool is_boolean(const LogicalType & type) noexcept
{
    return type.id == LogicalTypeId::Boolean;
}

bool is_varchar(const LogicalType & type) noexcept
{
    return type.id == LogicalTypeId::Varchar;
}

std::string type_name(const LogicalType & type)
{
    std::string name;
    switch (type.id) {
    case LogicalTypeId::Null: name = "NULL"; break;
    case LogicalTypeId::Boolean: name = "BOOLEAN"; break;
    case LogicalTypeId::Integer: name = "INTEGER"; break;
    case LogicalTypeId::BigInt: name = "BIGINT"; break;
    case LogicalTypeId::Float: name = "FLOAT"; break;
    case LogicalTypeId::Double: name = "DOUBLE"; break;
    case LogicalTypeId::Varchar: name = "VARCHAR"; break;
    case LogicalTypeId::Vector: name = "VECTOR"; break;
    }

    if (type.parameter.has_value()) {
        name += "(" + std::to_string(type.parameter.value()) + ")";
    }
    return name;
}

int numeric_rank(const LogicalType & type) noexcept
{
    switch (type.id) {
    case LogicalTypeId::Integer: return 1;
    case LogicalTypeId::BigInt: return 2;
    case LogicalTypeId::Float: return 3;
    case LogicalTypeId::Double: return 4;
    default: return 0;
    }
}

LogicalType common_numeric_type(
    const LogicalType & left,
    const LogicalType & right
) noexcept
{
    return numeric_rank(left) >= numeric_rank(right) ? left : right;
}

bool can_implicitly_cast(
    const LogicalType & source,
    const LogicalType & target
) noexcept
{
    return implicit_cast_cost(source, target).has_value();
}

std::optional<std::size_t> implicit_cast_cost(
    const LogicalType & source,
    const LogicalType & target
) noexcept
{
    if (same_type(source, target)) {
        return 0;
    }
    if (source.id == LogicalTypeId::Null) {
        return 1;
    }
    if (source.id == LogicalTypeId::Vector && target.id == LogicalTypeId::Vector) {
        if (!source.parameter.has_value()
            || !target.parameter.has_value()
            || source.parameter.value() == target.parameter.value()) {
            return 1;
        }
        return std::nullopt;
    }
    if (is_numeric(source) && is_numeric(target)
        && numeric_rank(source) <= numeric_rank(target)) {
        return static_cast<std::size_t>(10 + numeric_rank(target) - numeric_rank(source));
    }
    if (source.id == LogicalTypeId::Varchar
        && target.id == LogicalTypeId::Varchar) {
        return 1;
    }
    return std::nullopt;
}

bool can_compare(
    const LogicalType & left,
    const LogicalType & right,
    BinaryOperator op
) noexcept
{
    const auto equality = op == BinaryOperator::Equal
        || op == BinaryOperator::NotEqual;
    const auto ordered = op == BinaryOperator::LessThan
        || op == BinaryOperator::LessThanOrEqual
        || op == BinaryOperator::GreaterThan
        || op == BinaryOperator::GreaterThanOrEqual;

    if (equality) {
        return same_type(left, right)
            || (is_numeric(left) && is_numeric(right))
            || (is_varchar(left) && is_varchar(right));
    }
    return ordered
        && ((is_numeric(left) && is_numeric(right))
            || (is_varchar(left) && is_varchar(right)));
}

} // namespace litedb::core::common
