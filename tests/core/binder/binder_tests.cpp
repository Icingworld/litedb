#include "core/binder/binder.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/catalog/in_memory_catalog.hpp"
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{

using namespace litedb::core::binder;
using namespace litedb::core::binder::bound;
using namespace litedb::core::catalog;
using namespace litedb::core::common;
using namespace litedb::core::parser;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

std::unique_ptr<litedb::core::parser::ast::StatementNode> parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(std::string(result.error().message).append(": ").append(sql));
    }
    return std::move(result.value());
}

struct Fixture
{
    InMemoryCatalog catalog;
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        if (!database.has_value()) {
            throw std::runtime_error(database.error().message);
        }
        database_id = database.value();

        CreateCollectionRequest users;
        users.database_id = database_id;
        users.name = "users";
        users.columns = {
            ColumnDefinition {
                .name = "id",
                .type = type(LogicalTypeId::BigInt),
                .primary_key = true,
            },
            ColumnDefinition {
                .name = "name",
                .type = type(LogicalTypeId::Varchar, 64),
                .default_expression = CatalogDefaultExpression::literal(CatalogDefaultLiteralKind::String, "unknown"),
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
            throw std::runtime_error(collection.error().message);
        }
        users_id = collection.value();
    }
};

std::unique_ptr<BoundStatement> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    Binder binder {fixture.catalog, session};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

BinderError bind_error(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    Binder binder {fixture.catalog, session};
    auto result = binder.bind(*statement);
    require(!result.has_value(), "statement should fail to bind");
    return result.error();
}

void test_use_and_missing_database_context()
{
    Fixture fixture;
    auto use_statement = bind_ok(fixture, "USE demo;");
    require(use_statement->kind() == BoundStatementKind::Use, "USE kind mismatch");
    const auto * use = static_cast<const BoundUseStatement *>(use_statement.get());
    require(use->database_id() == fixture.database_id, "USE database id mismatch");

    auto select_ast = parse_ok("SELECT * FROM users;");
    SessionContext empty_session;
    Binder binder {fixture.catalog, empty_session};
    auto result = binder.bind(*select_ast);
    require(!result.has_value(), "SELECT without database should fail");
    require(result.error().code == BinderErrorCode::DatabaseNotSelected, "missing database error mismatch");
}

void test_select_binding()
{
    Fixture fixture;
    auto statement = bind_ok(
        fixture,
        "SELECT id, name FROM users WHERE age >= 18 ORDER BY age DESC LIMIT 10 OFFSET 20;"
    );

    require(statement->kind() == BoundStatementKind::Select, "SELECT kind mismatch");
    const auto * select = static_cast<const BoundSelectStatement *>(statement.get());
    require(select->collection_id() == fixture.users_id, "SELECT collection id mismatch");
    require(select->projections().size() == 2, "SELECT projection count mismatch");
    require(select->projections()[0]->kind() == BoundExpressionKind::ColumnRef, "SELECT projection kind mismatch");
    require(select->where() != nullptr, "SELECT where missing");
    require(select->where()->type().id == LogicalTypeId::Boolean, "SELECT where type mismatch");
    require(select->order_by().size() == 1, "SELECT order count mismatch");
    require(!select->order_by()[0].ascending, "SELECT order direction mismatch");
    require(select->limit().value() == 10, "SELECT limit mismatch");
    require(select->offset().value() == 20, "SELECT offset mismatch");

    auto wildcard = bind_ok(fixture, "SELECT * FROM users;");
    const auto * wildcard_select = static_cast<const BoundSelectStatement *>(wildcard.get());
    require(wildcard_select->projections().size() == 4, "wildcard expansion mismatch");

    auto varchar_comparison = bind_ok(fixture, "SELECT id FROM users WHERE name = 'test';");
    const auto * varchar_select = static_cast<const BoundSelectStatement *>(varchar_comparison.get());
    require(varchar_select->where() != nullptr, "VARCHAR comparison where missing");
    require(varchar_select->where()->type().id == LogicalTypeId::Boolean, "VARCHAR comparison should bind as boolean");
}

void test_select_errors()
{
    Fixture fixture;
    require(bind_error(fixture, "SELECT missing FROM users;").code == BinderErrorCode::ColumnNotFound, "missing column error mismatch");
    require(bind_error(fixture, "SELECT other.id FROM users;").code == BinderErrorCode::InvalidQualifier, "qualifier error mismatch");
    require(bind_error(fixture, "SELECT * FROM users WHERE name + 1 > 3;").code == BinderErrorCode::InvalidType, "invalid arithmetic error mismatch");

    litedb::core::parser::ast::FunctionCallExpression::ArgumentList arguments;
    litedb::core::parser::ast::SelectStatement::SelectList select_list;
    select_list.push_back(std::make_unique<litedb::core::parser::ast::FunctionCallExpression>(
        "distance",
        std::move(arguments),
        litedb::core::parser::ast::AstNodeLocation {1, 8}
    ));
    litedb::core::parser::ast::SelectStatement::OrderByList order_by;
    litedb::core::parser::ast::SelectStatement function_statement {
        std::move(select_list),
        "users",
        nullptr,
        std::move(order_by),
        std::nullopt,
        std::nullopt,
        litedb::core::parser::ast::AstNodeLocation {1, 1}
    };
    SessionContext session {.current_database_id = fixture.database_id};
    Binder binder {fixture.catalog, session};
    auto function_result = binder.bind(function_statement);
    require(!function_result.has_value(), "function call should fail to bind");
    require(function_result.error().code == BinderErrorCode::UnsupportedExpression, "function call error mismatch");
}

void test_insert_binding()
{
    Fixture fixture;
    auto statement = bind_ok(fixture, "INSERT INTO users (id, age, embedding) VALUES (1, 18, [0.1, 0.2, 0.3]);");
    require(statement->kind() == BoundStatementKind::Insert, "INSERT kind mismatch");
    const auto * insert = static_cast<const BoundInsertStatement *>(statement.get());
    require(insert->columns().size() == 4, "INSERT should bind full row");
    require(insert->values().size() == 4, "INSERT value count mismatch");
    require(insert->values()[1]->kind() == BoundExpressionKind::Cast, "INSERT default should cast to column type");
    require(insert->values()[3]->type().id == LogicalTypeId::Vector, "INSERT vector type mismatch");
    require(insert->values()[3]->type().parameter.value() == 3, "INSERT vector dimension mismatch");

    auto all_columns = bind_ok(fixture, "INSERT INTO users VALUES (1, 'Tom', 18, [0.1, 0.2, 0.3]);");
    const auto * full_insert = static_cast<const BoundInsertStatement *>(all_columns.get());
    require(full_insert->values().size() == 4, "full INSERT value count mismatch");
}

void test_insert_errors()
{
    Fixture fixture;
    require(bind_error(fixture, "INSERT INTO users (id, id) VALUES (1, 2);").code == BinderErrorCode::DuplicateColumn, "duplicate insert column error mismatch");
    require(bind_error(fixture, "INSERT INTO users (id) VALUES (1, 2);").code == BinderErrorCode::InvalidValueCount, "insert value count error mismatch");
    require(bind_error(fixture, "INSERT INTO users (id, embedding) VALUES (1, [0.1, 0.2]);").code == BinderErrorCode::InvalidType, "vector dimension error mismatch");
    require(bind_error(fixture, "INSERT INTO users (id) VALUES (NULL);").code == BinderErrorCode::NotNullable, "insert null primary key error mismatch");
}

void test_update_delete_binding()
{
    Fixture fixture;
    auto update = bind_ok(fixture, "UPDATE users SET age = age + 1 WHERE id = 1;");
    require(update->kind() == BoundStatementKind::Update, "UPDATE kind mismatch");
    const auto * bound_update = static_cast<const BoundUpdateStatement *>(update.get());
    require(bound_update->assignments().size() == 1, "UPDATE assignment count mismatch");
    require(bound_update->where() != nullptr, "UPDATE where missing");

    auto del = bind_ok(fixture, "DELETE FROM users;");
    require(del->kind() == BoundStatementKind::Delete, "DELETE kind mismatch");
    const auto * bound_delete = static_cast<const BoundDeleteStatement *>(del.get());
    require(bound_delete->where() == nullptr, "DELETE without where mismatch");

    require(bind_error(fixture, "UPDATE users SET age = name;").code == BinderErrorCode::InvalidType, "UPDATE type error mismatch");
    require(bind_error(fixture, "UPDATE users SET id = NULL;").code == BinderErrorCode::NotNullable, "UPDATE null primary key error mismatch");
    require(bind_error(fixture, "DELETE FROM users WHERE age + 1;").code == BinderErrorCode::InvalidType, "DELETE where type error mismatch");
}

void test_ddl_and_metadata_binding()
{
    Fixture fixture;
    require(bind_ok(fixture, "CREATE DATABASE demo2;")->kind() == BoundStatementKind::CreateDatabase, "CREATE DATABASE kind mismatch");
    require(bind_ok(fixture, "DROP DATABASE IF EXISTS missing;")->kind() == BoundStatementKind::DropDatabase, "DROP DATABASE IF EXISTS kind mismatch");
    require(bind_ok(fixture, "SHOW DATABASES;")->kind() == BoundStatementKind::ShowDatabases, "SHOW DATABASES kind mismatch");
    require(bind_ok(fixture, "SHOW COLLECTIONS;")->kind() == BoundStatementKind::ShowCollections, "SHOW COLLECTIONS kind mismatch");
    require(bind_ok(fixture, "DESCRIBE users;")->kind() == BoundStatementKind::DescribeCollection, "DESCRIBE kind mismatch");

    auto create = bind_ok(fixture, "CREATE COLLECTION posts (id BIGINT PRIMARY KEY, embedding VECTOR(3));");
    require(create->kind() == BoundStatementKind::CreateCollection, "CREATE COLLECTION kind mismatch");
    const auto * create_collection = static_cast<const BoundCreateCollectionStatement *>(create.get());
    require(create_collection->columns().size() == 2, "CREATE COLLECTION column count mismatch");
    require(create_collection->columns()[1].type.id == LogicalTypeId::Vector, "CREATE COLLECTION vector type mismatch");

    require(bind_error(fixture, "CREATE COLLECTION bad (id BIGINT PRIMARY KEY, other BIGINT PRIMARY KEY);").code == BinderErrorCode::DuplicatePrimaryKey, "duplicate primary key error mismatch");
    require(bind_error(fixture, "CREATE COLLECTION bad_default (age INTEGER DEFAULT 'old');").code == BinderErrorCode::InvalidType, "default type error mismatch");
}

void test_index_binding()
{
    Fixture fixture;
    auto create_age = bind_ok(fixture, "CREATE INDEX idx_age ON users (age);");
    require(create_age->kind() == BoundStatementKind::CreateIndex, "CREATE INDEX kind mismatch");
    const auto * bound_create_age = static_cast<const BoundCreateIndexStatement *>(create_age.get());
    require(bound_create_age->database_id() == fixture.database_id, "CREATE INDEX database id mismatch");
    require(bound_create_age->collection_id() == fixture.users_id, "CREATE INDEX collection id mismatch");
    require(bound_create_age->collection_name() == "users", "CREATE INDEX collection name mismatch");
    require(bound_create_age->column_name() == "age", "CREATE INDEX column name mismatch");
    require(bound_create_age->index_name() == "idx_age", "CREATE INDEX index name mismatch");
    require(bound_create_age->index_kind() == CatalogIndexKind::BTree, "CREATE INDEX default kind mismatch");
    require(!bound_create_age->unique(), "CREATE INDEX unique mismatch");
    require(!bound_create_age->if_not_exists(), "CREATE INDEX if-not-exists mismatch");

    auto create_name = bind_ok(fixture, "CREATE INDEX IF NOT EXISTS idx_name ON users (name) USING HASH;");
    const auto * bound_create_name = static_cast<const BoundCreateIndexStatement *>(create_name.get());
    require(bound_create_name->index_kind() == CatalogIndexKind::Hash, "CREATE INDEX hash kind mismatch");
    require(bound_create_name->if_not_exists(), "CREATE INDEX IF NOT EXISTS mismatch");

    require(bind_error(fixture, "CREATE INDEX idx_embedding ON users (embedding);").code == BinderErrorCode::InvalidType, "vector index type error mismatch");
    require(bind_error(fixture, "CREATE INDEX idx_missing ON users (missing);").code == BinderErrorCode::ColumnNotFound, "missing index column error mismatch");

    const auto * age_column = fixture.catalog.find_column(fixture.users_id, "age");
    require(age_column != nullptr, "age column lookup failed");
    auto created_index = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = age_column->id(),
        .name = "idx_age",
        .index_kind = CatalogIndexKind::BTree,
    });
    require(created_index.has_value(), "fixture index create failed");

    auto drop_age = bind_ok(fixture, "DROP INDEX idx_age ON users;");
    require(drop_age->kind() == BoundStatementKind::DropIndex, "DROP INDEX kind mismatch");
    const auto * bound_drop_age = static_cast<const BoundDropIndexStatement *>(drop_age.get());
    require(bound_drop_age->database_id() == fixture.database_id, "DROP INDEX database id mismatch");
    require(bound_drop_age->collection_id() == fixture.users_id, "DROP INDEX collection id mismatch");
    require(bound_drop_age->collection_name() == "users", "DROP INDEX collection name mismatch");
    require(bound_drop_age->index_name() == "idx_age", "DROP INDEX index name mismatch");
    require(!bound_drop_age->if_exists(), "DROP INDEX if-exists mismatch");

    require(bind_error(fixture, "DROP INDEX missing ON users;").code == BinderErrorCode::IndexNotFound, "missing index error mismatch");

    auto drop_missing = bind_ok(fixture, "DROP INDEX IF EXISTS missing ON users;");
    const auto * bound_drop_missing = static_cast<const BoundDropIndexStatement *>(drop_missing.get());
    require(bound_drop_missing->if_exists(), "DROP INDEX IF EXISTS mismatch");
}

} // namespace

int main()
{
    try {
        test_use_and_missing_database_context();
        test_select_binding();
        test_select_errors();
        test_insert_binding();
        test_insert_errors();
        test_update_delete_binding();
        test_ddl_and_metadata_binding();
        test_index_binding();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
