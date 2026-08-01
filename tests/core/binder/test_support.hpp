#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/binder/binder.hpp"
#include "core/binder/binder_error.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/schema/default_expression.hpp"

namespace litedb::test::binder
{

using namespace core::binder;
using namespace core::binder::bound;
using namespace core::common;
using namespace core::meta;
using namespace core::parser;

inline void require(bool condition, std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

inline LogicalType type(
    LogicalTypeId id,
    std::optional<std::size_t> parameter = std::nullopt
)
{
    return LogicalType {id, parameter};
}

inline std::unique_ptr<core::parser::ast::StatementNode> parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(
            std::string(result.error().message()).append(": ").append(sql)
        );
    }
    return std::move(*result);
}

struct Fixture
{
    CatalogEditor catalog;
    DatabaseId database_id {0};
    CollectionId users_id {0};
    ColumnId id_column_id {0};
    ColumnId name_column_id {0};
    ColumnId age_column_id {0};
    ColumnId embedding_column_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        if (!database.has_value()) {
            throw std::runtime_error(database.error().message());
        }
        database_id = *database;

        CreateCollectionRequest users;
        users.database_id = database_id;
        users.name = "users";
        users.columns = {
            ColumnDefinition {
                .name = "id",
                .type = type(LogicalTypeId::BigInt),
                .nullable = false,
            },
            ColumnDefinition {
                .name = "name",
                .type = type(LogicalTypeId::Varchar, 64),
                .default_expression = core::schema::DefaultExpression::literal(
                    core::schema::DefaultLiteralKind::String,
                    "unknown"
                ),
            },
            ColumnDefinition {
                .name = "age",
                .type = type(LogicalTypeId::Integer),
                .nullable = true,
            },
            ColumnDefinition {
                .name = "embedding",
                .type = type(LogicalTypeId::Vector, 3),
                .nullable = true,
            },
        };

        auto collection = catalog.create_collection(users);
        if (!collection.has_value()) {
            throw std::runtime_error(collection.error().message());
        }
        users_id = *collection;

        id_column_id = require_column("id").id();
        name_column_id = require_column("name").id();
        age_column_id = require_column("age").id();
        embedding_column_id = require_column("embedding").id();
    }

    const core::meta::entry::ColumnEntry & require_column(std::string_view name) const
    {
        const auto * column = catalog.view().find_column(users_id, name);
        if (column == nullptr) {
            throw std::runtime_error("fixture column not found: " + std::string(name));
        }
        return *column;
    }
};

inline std::unique_ptr<BoundStatement> bind_statement(
    Fixture & fixture,
    std::string_view sql,
    std::optional<DatabaseId> current_database
)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = current_database};
    BinderContext context {fixture.catalog.view(), session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(
            std::string(result.error().message()).append(": ").append(sql)
        );
    }
    return std::move(*result);
}

inline std::unique_ptr<BoundStatement> bind_statement(
    Fixture & fixture,
    std::string_view sql
)
{
    return bind_statement(fixture, sql, fixture.database_id);
}

template <typename T>
std::unique_ptr<T> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = bind_statement(fixture, sql);
    auto * typed = dynamic_cast<T *>(statement.get());
    require(typed != nullptr, "bound statement type mismatch");
    statement.release();
    return std::unique_ptr<T>(typed);
}

inline BinderError bind_error(
    Fixture & fixture,
    std::string_view sql,
    std::optional<DatabaseId> current_database
)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = current_database};
    BinderContext context {fixture.catalog.view(), session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    require(!result.has_value(), "statement should fail to bind");
    return std::move(result.error());
}

inline BinderError bind_error(Fixture & fixture, std::string_view sql)
{
    return bind_error(fixture, sql, fixture.database_id);
}

inline BinderError require_error(
    Fixture & fixture,
    std::string_view sql,
    BinderErrorCode code
)
{
    auto error = bind_error(fixture, sql);
    require(error.is(code), "binder error code mismatch");
    return error;
}

} // namespace litedb::test::binder
