#include "core/binder/binder_helper.hpp"

#include <utility>

#include "core/binder/bound/expression/bound_cast_expression.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderError make_binder_error(BinderErrorCode code, AstNodeLocation location, std::string message)
{
    return BinderError {
        .code = code,
        .location = location,
        .message = std::move(message),
    };
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter)
{
    return LogicalType {id, parameter};
}

bool same_type(const LogicalType & left, const LogicalType & right)
{
    return left.id == right.id && left.parameter == right.parameter;
}

bool is_numeric(const LogicalType & value)
{
    return value.id == LogicalTypeId::Integer
        || value.id == LogicalTypeId::BigInt
        || value.id == LogicalTypeId::Float
        || value.id == LogicalTypeId::Double;
}

bool is_boolean(const LogicalType & value)
{
    return value.id == LogicalTypeId::Boolean;
}

bool is_varchar(const LogicalType & value)
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

int numeric_rank(const LogicalType & value)
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

LogicalType common_numeric_type(const LogicalType & left, const LogicalType & right)
{
    return numeric_rank(left) >= numeric_rank(right) ? left : right;
}

bool can_cast(const LogicalType & source, const LogicalType & target)
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

    if (source.id == LogicalTypeId::Varchar && target.id == LogicalTypeId::Varchar) {
        return true;
    }

    if (source.id == LogicalTypeId::Vector && target.id == LogicalTypeId::Vector) {
        return !source.parameter.has_value()
            || !target.parameter.has_value()
            || source.parameter.value() == target.parameter.value();
    }

    return false;
}

bool can_compare(const LogicalType & left, const LogicalType & right, TokenType op)
{
    if (is_numeric(left) && is_numeric(right)) {
        return true;
    }

    if (is_varchar(left) && is_varchar(right)) {
        return true;
    }

    if (same_type(left, right)) {
        if (op == TokenType::Equal || op == TokenType::NotEqual) {
            return true;
        }
        return left.id == LogicalTypeId::Varchar
            || left.id == LogicalTypeId::Boolean
            || is_numeric(left);
    }

    return false;
}

std::unique_ptr<BoundExpression> cast_if_needed(std::unique_ptr<BoundExpression> expression, LogicalType target_type)
{
    if (same_type(expression->type(), target_type)) {
        return expression;
    }
    const auto location = expression->location();
    return std::make_unique<BoundCastExpression>(std::move(expression), target_type, location);
}

BoundColumn bound_column_from_entry(const catalog::ColumnEntry & column)
{
    return BoundColumn {
        .column_id = column.id(),
        .name = column.name(),
        .type = column.type(),
        .nullable = column.nullable(),
    };
}

catalog::CatalogIndexKind catalog_index_kind(CreateIndexMethod method)
{
    switch (method) {
    case CreateIndexMethod::Default:
        [[fallthrough]];
    case CreateIndexMethod::BTree:
        return catalog::CatalogIndexKind::BTree;
    }

    return catalog::CatalogIndexKind::BTree;
}

} // namespace litedb::core::binder
