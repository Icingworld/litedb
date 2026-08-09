#include "core/binder/detail/column_definition_binder.hpp"

#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include "core/binder/binder_helper.hpp"
#include "core/binder/detail/default_expression_binder.hpp"
#include "core/common/identifier.hpp"

namespace litedb::core::binder::detail
{

using namespace litedb::core::common;

namespace
{

[[nodiscard]]
std::expected<LogicalType, BinderError> validate_data_type(const LogicalType & data_type)
{
    switch (data_type.id) {
    case LogicalTypeId::Null:
        return std::unexpected(
            make_binder_error(BinderErrorCode::InvalidType, "NULL is not a valid column type")
        );
    case LogicalTypeId::Integer:
    case LogicalTypeId::BigInt:
    case LogicalTypeId::Float:
    case LogicalTypeId::Double:
    case LogicalTypeId::Boolean:
        if (data_type.parameter.has_value()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                "Scalar type cannot have a parameter"
            ));
        }
        return data_type;
    case LogicalTypeId::Varchar:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(
                make_binder_error(BinderErrorCode::InvalidType, "VARCHAR length must be positive")
            );
        }
        return data_type;
    case LogicalTypeId::Vector:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(
                make_binder_error(BinderErrorCode::InvalidType, "VECTOR dimension must be positive")
            );
        }
        return data_type;
    }
    [[unlikely]] return std::unexpected(
        make_binder_error(BinderErrorCode::InvalidType, "Unsupported data type")
    );
}

} // namespace

std::expected<std::vector<meta::ColumnDefinition>, BinderError> bind_column_definitions(
    const std::vector<std::unique_ptr<parser::ast::ColumnDefinitionSyntax>> & columns
)
{
    std::unordered_set<std::string> seen_columns;
    std::vector<meta::ColumnDefinition> result;
    result.reserve(columns.size());

    for (const auto & column_ptr : columns) {
        const auto & column = *column_ptr;
        if (!seen_columns.emplace(common::normalize_identifier(column.name)).second) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DuplicateColumn,
                column.location,
                "Duplicate column: " + column.name
            ));
        }
        auto logical_type = validate_data_type(column.type);
        if (!logical_type.has_value()) [[unlikely]] {
            return std::unexpected(std::move(logical_type.error()));
        }

        std::optional<schema::DefaultExpression> default_expression;
        if (column.default_value != nullptr) {
            auto default_snapshot = snapshot_default_expression(*column.default_value);
            if (!default_snapshot.has_value()) [[unlikely]] {
                return std::unexpected(std::move(default_snapshot.error()));
            }
            default_expression = std::move(*default_snapshot);

            auto bound_default = bind_default_expression(*default_expression);
            if (!bound_default.has_value()) [[unlikely]] {
                return std::unexpected(std::move(bound_default.error()));
            }
            if ((*bound_default)->type().id == LogicalTypeId::Null && !column.nullable)
                [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::NotNullable,
                    column.default_value->location(),
                    "NOT NULL column cannot have a NULL default: " + column.name
                ));
            }
            if (!can_cast((*bound_default)->type(), *logical_type)) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::InvalidType,
                    column.default_value->location(),
                    "DEFAULT value type " + type_name((*bound_default)->type()) +
                        " does not match column " + column.name + " type " +
                        type_name(*logical_type)
                ));
            }
        }

        result.push_back(
            meta::ColumnDefinition {
                .name = column.name,
                .type = *logical_type,
                .unique = column.unique,
                .nullable = column.nullable,
                .default_expression = std::move(default_expression),
                .comment = column.comment,
            }
        );
    }

    return result;
}

} // namespace litedb::core::binder::detail
