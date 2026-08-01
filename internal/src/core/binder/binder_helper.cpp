#include "core/binder/binder_helper.hpp"

#include <utility>

#include "core/binder/bound/expression/bound_cast_expression.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;

BinderError make_binder_error(
    BinderErrorCode code,
    std::string_view message
)
{
    return BinderError {code, message};
}

BinderError make_binder_error(
    BinderErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string_view message
)
{
    return BinderError {code, message, BinderErrorContext {location}};
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter) noexcept
{
    return LogicalType {id, parameter};
}

bool same_type(const LogicalType & left, const LogicalType & right) noexcept
{
    return left.id == right.id && left.parameter == right.parameter;
}

bool is_numeric(const LogicalType & value) noexcept
{
    return value.id == LogicalTypeId::Integer
        || value.id == LogicalTypeId::BigInt
        || value.id == LogicalTypeId::Float
        || value.id == LogicalTypeId::Double;
}

bool is_boolean(const LogicalType & value) noexcept
{
    return value.id == LogicalTypeId::Boolean;
}

bool is_varchar(const LogicalType & value) noexcept
{
    return value.id == LogicalTypeId::Varchar;
}

std::string type_name(const LogicalType & value)
{
    std::string name;
    switch (value.id) {
    case LogicalTypeId::Null:
        name = "NULL";
        break;
    case LogicalTypeId::Boolean:
        name = "BOOLEAN";
        break;
    case LogicalTypeId::Integer:
        name = "INTEGER";
        break;
    case LogicalTypeId::BigInt:
        name = "BIGINT";
        break;
    case LogicalTypeId::Float:
        name = "FLOAT";
        break;
    case LogicalTypeId::Double:
        name = "DOUBLE";
        break;
    case LogicalTypeId::Varchar:
        name = "VARCHAR";
        break;
    case LogicalTypeId::Vector:
        name = "VECTOR";
        break;
    }

    if (value.parameter.has_value()) {
        name += "(" + std::to_string(value.parameter.value()) + ")";
    }
    return name;
}

int numeric_rank(const LogicalType & value) noexcept
{
    switch (value.id) {
    case LogicalTypeId::Integer:
        return 1;
    case LogicalTypeId::BigInt:
        return 2;
    case LogicalTypeId::Float:
        return 3;
    case LogicalTypeId::Double:
        return 4;
    default:
        return 0;
    }
}

LogicalType common_numeric_type(
    const LogicalType & left,
    const LogicalType & right
) noexcept
{
    return numeric_rank(left) >= numeric_rank(right) ? left : right;
}

bool can_cast(const LogicalType & source, const LogicalType & target) noexcept
{
    if (source.id == LogicalTypeId::Null) {
        return true;
    }

    if (same_type(source, target)) {
        return true;
    }

    if (is_numeric(source) && is_numeric(target)) {
        return numeric_rank(source) <= numeric_rank(target);
    }

    if (source.id == LogicalTypeId::Varchar
        && target.id == LogicalTypeId::Varchar) {
        return true;
    }

    if (source.id == LogicalTypeId::Vector
        && target.id == LogicalTypeId::Vector) {
        return !source.parameter.has_value()
            || !target.parameter.has_value()
            || source.parameter.value() == target.parameter.value();
    }

    return false;
}

bool can_compare(
    const LogicalType & left,
    const LogicalType & right,
    BinaryOperator op
) noexcept
{
    const bool equality = op == BinaryOperator::Equal
        || op == BinaryOperator::NotEqual;
    const bool ordered = op == BinaryOperator::LessThan
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

std::unique_ptr<BoundExpression> cast_if_needed(
    std::unique_ptr<BoundExpression> expression,
    LogicalType target_type
)
{
    if (same_type(expression->type(), target_type)) {
        return expression;
    }
    return std::make_unique<BoundCastExpression>(
        std::move(expression),
        target_type
    );
}

BoundColumn bound_column_from_entry(const meta::entry::ColumnEntry & column)
{
    return BoundColumn {
        .column_id = column.id(),
        .name = column.name(),
        .type = column.type(),
        .nullable = column.nullable(),
    };
}

} // namespace litedb::core::binder
