#include "core/binder/binder_helper.hpp"

#include <utility>

#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/common/type_rules.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;

BinderError make_binder_error(
    BinderErrorCode code,
    std::string_view message
)
{
    return BinderError {
        code,
        message
    };
}

BinderError make_binder_error(
    BinderErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string_view message
)
{
    return BinderError {
        code,
        message,
        BinderErrorContext {
            location
        }
    };
}

LogicalType type(
    LogicalTypeId id,
    std::optional<std::size_t> parameter
) noexcept
{
    return LogicalType {
        id,
        parameter
    };
}

bool same_type(
    const LogicalType & left,
    const LogicalType & right
) noexcept
{
    return common::same_type(left, right);
}

bool is_numeric(const LogicalType & value) noexcept
{
    return common::is_numeric(value);
}

bool is_boolean(const LogicalType & value) noexcept
{
    return common::is_boolean(value);
}

bool is_varchar(const LogicalType & value) noexcept
{
    return common::is_varchar(value);
}

std::string type_name(const LogicalType & value)
{
    return common::type_name(value);
}

int numeric_rank(const LogicalType & value) noexcept
{
    return common::numeric_rank(value);
}

LogicalType common_numeric_type(
    const LogicalType & left,
    const LogicalType & right
) noexcept
{
    return common::common_numeric_type(left, right);
}

bool can_cast(
    const LogicalType & source,
    const LogicalType & target
) noexcept
{
    return common::can_implicitly_cast(source, target);
}

bool can_compare(
    const LogicalType & left,
    const LogicalType & right,
    BinaryOperator op
) noexcept
{
    return common::can_compare(left, right, op);
}

std::unique_ptr<BoundExpression> cast_if_needed(
    std::unique_ptr<BoundExpression> expression,
    LogicalType target_type
)
{
    if (common::same_type(expression->type(), target_type)) {
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
