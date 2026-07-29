#include "core/binder/binder.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/meta/meta_engine.hpp"
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
using namespace litedb::core::meta;
using namespace litedb::core::meta::entry;
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
        throw std::runtime_error(std::string(result.error().message()).append(": ").append(sql));
    }
    return std::move(*result);
}

struct Fixture
{
    CatalogEditor catalog;
    DatabaseId database_id {0};
    CollectionId users_id {0};

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
                .default_expression = litedb::core::schema::DefaultExpression::literal(litedb::core::schema::DefaultLiteralKind::String, "unknown"),
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
    }
};

std::unique_ptr<BoundStatement> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    BinderContext context {fixture.catalog.view(), session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

BinderError bind_error(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    BinderContext context {fixture.catalog.view(), session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    require(!result.has_value(), "statement should fail to bind");
    return std::move(result.error());
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
    BinderContext context {fixture.catalog.view(), empty_session};
    Binder binder {context};
    auto result = binder.bind(*select_ast);
    require(!result.has_value(), "SELECT without database should fail");
    require(result.error().is(BinderErrorCode::DatabaseNotSelected), "missing database error mismatch");
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
    require(select->projections()[0].expression->kind() == BoundExpressionKind::ColumnRef, "SELECT projection kind mismatch");
    require(!select->projections()[0].alias.has_value(), "SELECT projection alias mismatch");
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

void test_select_alias_binding()
{
    Fixture fixture;

    auto expression_alias = bind_ok(fixture, "SELECT age + 1 AS next_age FROM users;");
    const auto * expression_select = static_cast<const BoundSelectStatement *>(expression_alias.get());
    require(expression_select->projections().size() == 1, "SELECT alias projection count mismatch");
    require(expression_select->projections()[0].alias.has_value(), "SELECT alias missing");
    require(expression_select->projections()[0].alias.value() == "next_age", "SELECT alias name mismatch");
    require(expression_select->projections()[0].expression->kind() == BoundExpressionKind::Binary, "SELECT alias expression kind mismatch");

    auto order_by_alias = bind_ok(fixture, "SELECT age + 1 AS next_age FROM users ORDER BY next_age DESC;");
    const auto * order_by_select = static_cast<const BoundSelectStatement *>(order_by_alias.get());
    require(order_by_select->order_by().size() == 1, "ORDER BY alias count mismatch");
    require(!order_by_select->order_by()[0].ascending, "ORDER BY alias direction mismatch");
    require(order_by_select->order_by()[0].expression->kind() == BoundExpressionKind::Binary, "ORDER BY alias expression kind mismatch");

    auto alias_shadows_column = bind_ok(fixture, "SELECT name AS age FROM users ORDER BY age;");
    const auto * shadow_select = static_cast<const BoundSelectStatement *>(alias_shadows_column.get());
    require(shadow_select->order_by()[0].expression->kind() == BoundExpressionKind::ColumnRef, "ORDER BY shadow alias kind mismatch");
    require(shadow_select->order_by()[0].expression->type().id == LogicalTypeId::Varchar, "ORDER BY should prefer alias over source column");

    auto duplicate_alias = bind_ok(fixture, "SELECT age AS x, name AS x FROM users;");
    const auto * duplicate_select = static_cast<const BoundSelectStatement *>(duplicate_alias.get());
    require(duplicate_select->projections().size() == 2, "duplicate alias projection count mismatch");
    require(duplicate_select->projections()[0].alias.value() == "x", "first duplicate alias mismatch");
    require(duplicate_select->projections()[1].alias.value() == "x", "second duplicate alias mismatch");

    require(bind_error(fixture, "SELECT age AS x, name AS x FROM users ORDER BY x;").is(BinderErrorCode::AmbiguousAlias), "ambiguous ORDER BY alias error mismatch");
}

void test_select_errors()
{
    Fixture fixture;
    require(bind_error(fixture, "SELECT missing FROM users;").is(BinderErrorCode::ColumnNotFound), "missing column error mismatch");
    require(bind_error(fixture, "SELECT other.id FROM users;").is(BinderErrorCode::InvalidQualifier), "qualifier error mismatch");
    require(bind_error(fixture, "SELECT * FROM users WHERE name + 1 > 3;").is(BinderErrorCode::InvalidType), "invalid arithmetic error mismatch");

    require(bind_error(fixture, "SELECT missing_function(age) FROM users;").is(BinderErrorCode::UnsupportedExpression), "unknown function error mismatch");
}

void test_function_binding()
{
    Fixture fixture;
    auto statement = bind_ok(
        fixture,
        "SELECT id FROM users ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC LIMIT 3;"
    );

    const auto * select = static_cast<const BoundSelectStatement *>(statement.get());
    require(select->order_by().size() == 1, "function ORDER BY count mismatch");
    require(select->order_by()[0].expression->kind() == BoundExpressionKind::Function, "ORDER BY should bind function");
    const auto & function = static_cast<const BoundFunctionExpression &>(*select->order_by()[0].expression);
    require(function.name() == "l2_distance", "function name mismatch");
    require(function.type().id == LogicalTypeId::Double, "function return type mismatch");
    require(function.arguments().size() == 2, "function argument count mismatch");

    require(bind_error(fixture, "SELECT id FROM users ORDER BY l2_distance(embedding, [0.1, 0.2]);").is(BinderErrorCode::InvalidType), "function vector dimension error mismatch");
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
    require(bind_error(fixture, "INSERT INTO users (id, id) VALUES (1, 2);").is(BinderErrorCode::DuplicateColumn), "duplicate insert column error mismatch");
    require(bind_error(fixture, "INSERT INTO users (id) VALUES (1, 2);").is(BinderErrorCode::InvalidValueCount), "insert value count error mismatch");
    require(bind_error(fixture, "INSERT INTO users (id, embedding) VALUES (1, [0.1, 0.2]);").is(BinderErrorCode::InvalidType), "vector dimension error mismatch");
    require(bind_error(fixture, "INSERT INTO users (id) VALUES (NULL);").is(BinderErrorCode::NotNullable), "insert null primary key error mismatch");
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

    require(bind_error(fixture, "UPDATE users SET age = name;").is(BinderErrorCode::InvalidType), "UPDATE type error mismatch");
    require(bind_error(fixture, "UPDATE users SET id = NULL;").is(BinderErrorCode::NotNullable), "UPDATE null primary key error mismatch");
    require(bind_error(fixture, "DELETE FROM users WHERE age + 1;").is(BinderErrorCode::InvalidType), "DELETE where type error mismatch");
}

void test_ddl_and_metadata_binding()
{
    Fixture fixture;
    require(bind_ok(fixture, "CREATE DATABASE demo2;")->kind() == BoundStatementKind::CreateDatabase, "CREATE DATABASE kind mismatch");
    require(bind_ok(fixture, "DROP DATABASE IF EXISTS missing;")->kind() == BoundStatementKind::DropDatabase, "DROP DATABASE IF EXISTS kind mismatch");
    require(bind_ok(fixture, "SHOW DATABASES;")->kind() == BoundStatementKind::ShowDatabases, "SHOW DATABASES kind mismatch");
    require(bind_ok(fixture, "SHOW COLLECTIONS;")->kind() == BoundStatementKind::ShowCollections, "SHOW COLLECTIONS kind mismatch");
    auto show_indexes = bind_ok(fixture, "SHOW INDEXES FROM users;");
    require(show_indexes->kind() == BoundStatementKind::ShowIndexes, "SHOW INDEXES kind mismatch");
    const auto * bound_show_indexes = static_cast<const BoundShowIndexesStatement *>(show_indexes.get());
    require(bound_show_indexes->database_id() == fixture.database_id, "SHOW INDEXES database id mismatch");
    require(bound_show_indexes->collection_id() == fixture.users_id, "SHOW INDEXES collection id mismatch");
    require(bound_show_indexes->collection_name() == "users", "SHOW INDEXES collection name mismatch");

    auto show_vector_indexes = bind_ok(fixture, "SHOW VINDEXES FROM users;");
    require(show_vector_indexes->kind() == BoundStatementKind::ShowVectorIndexes, "SHOW VINDEXES kind mismatch");
    const auto * bound_show_vector_indexes = static_cast<const BoundShowVectorIndexesStatement *>(show_vector_indexes.get());
    require(bound_show_vector_indexes->database_id() == fixture.database_id, "SHOW VINDEXES database id mismatch");
    require(bound_show_vector_indexes->collection_id() == fixture.users_id, "SHOW VINDEXES collection id mismatch");
    require(bound_show_vector_indexes->collection_name() == "users", "SHOW VINDEXES collection name mismatch");
    require(bind_ok(fixture, "DESCRIBE users;")->kind() == BoundStatementKind::DescribeCollection, "DESCRIBE kind mismatch");

    auto create = bind_ok(fixture, "CREATE COLLECTION posts (id BIGINT NOT NULL, embedding VECTOR(3) NULL);");
    require(create->kind() == BoundStatementKind::CreateCollection, "CREATE COLLECTION kind mismatch");
    const auto * create_collection = static_cast<const BoundCreateCollectionStatement *>(create.get());
    require(create_collection->columns().size() == 2, "CREATE COLLECTION column count mismatch");
    require(!create_collection->columns()[0].nullable, "CREATE COLLECTION NOT NULL mismatch");
    require(create_collection->columns()[1].type.id == LogicalTypeId::Vector, "CREATE COLLECTION vector type mismatch");
    require(create_collection->columns()[1].nullable, "CREATE COLLECTION NULL mismatch");

    require(bind_error(fixture, "CREATE COLLECTION bad_default (age INTEGER DEFAULT 'old');").is(BinderErrorCode::InvalidType), "default type error mismatch");
    require(bind_error(fixture, "CREATE COLLECTION bad_varchar (name VARCHAR(0));").is(BinderErrorCode::InvalidType), "VARCHAR length error mismatch");
    require(bind_error(fixture, "CREATE COLLECTION bad_vector (embedding VECTOR(0));").is(BinderErrorCode::InvalidType), "VECTOR dimension error mismatch");
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
    require(bound_create_age->index_kind() == IndexKind::BTree, "CREATE INDEX default kind mismatch");
    require(!bound_create_age->unique(), "CREATE INDEX unique mismatch");
    require(!bound_create_age->if_not_exists(), "CREATE INDEX if-not-exists mismatch");

    auto create_name = bind_ok(fixture, "CREATE INDEX IF NOT EXISTS idx_name ON users (name) USING BTREE;");
    const auto * bound_create_name = static_cast<const BoundCreateIndexStatement *>(create_name.get());
    require(bound_create_name->index_kind() == IndexKind::BTree, "CREATE INDEX BTREE kind mismatch");
    require(bound_create_name->if_not_exists(), "CREATE INDEX IF NOT EXISTS mismatch");

    require(bind_error(fixture, "CREATE INDEX idx_embedding ON users (embedding);").is(BinderErrorCode::InvalidType), "vector index type error mismatch");
    require(bind_error(fixture, "CREATE INDEX idx_missing ON users (missing);").is(BinderErrorCode::ColumnNotFound), "missing index column error mismatch");

    const auto * age_column = fixture.catalog.view().find_column(fixture.users_id, "age");
    require(age_column != nullptr, "age column lookup failed");
    auto created_index = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_ids = {age_column->id()},
        .name = "idx_age",
        .kind = IndexKind::BTree,
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

    require(bind_error(fixture, "DROP INDEX missing ON users;").is(BinderErrorCode::IndexNotFound), "missing index error mismatch");

    auto drop_missing = bind_ok(fixture, "DROP INDEX IF EXISTS missing ON users;");
    const auto * bound_drop_missing = static_cast<const BoundDropIndexStatement *>(drop_missing.get());
    require(bound_drop_missing->if_exists(), "DROP INDEX IF EXISTS mismatch");
}

void test_vector_index_binding()
{
    Fixture fixture;
    auto create = bind_ok(
        fixture,
        "CREATE VINDEX IF NOT EXISTS vidx_embedding ON users (embedding) USING HNSW "
        "WITH (metric = INNER_PRODUCT, max_neighbors = 24, ef_construction = 240, ef_search = 80, random_seed = 9);"
    );
    require(create->kind() == BoundStatementKind::CreateVectorIndex, "CREATE VINDEX kind mismatch");
    const auto * bound_create = static_cast<const BoundCreateVectorIndexStatement *>(create.get());
    require(bound_create->database_id() == fixture.database_id, "CREATE VINDEX database id mismatch");
    require(bound_create->collection_id() == fixture.users_id, "CREATE VINDEX collection id mismatch");
    require(bound_create->collection_name() == "users", "CREATE VINDEX collection name mismatch");
    require(bound_create->column_name() == "embedding", "CREATE VINDEX column name mismatch");
    require(bound_create->index_name() == "vidx_embedding", "CREATE VINDEX index name mismatch");
    require(bound_create->index_kind() == VectorIndexKind::Hnsw, "CREATE VINDEX kind value mismatch");
    require(bound_create->metric() == VectorDistanceMetric::InnerProduct, "CREATE VINDEX metric mismatch");
    require(bound_create->max_neighbors() == 24, "CREATE VINDEX max_neighbors mismatch");
    require(bound_create->ef_construction() == 240, "CREATE VINDEX ef_construction mismatch");
    require(bound_create->ef_search_default() == 80, "CREATE VINDEX ef_search mismatch");
    require(bound_create->random_seed() == 9, "CREATE VINDEX random_seed mismatch");
    require(bound_create->if_not_exists(), "CREATE VINDEX IF NOT EXISTS mismatch");

    auto defaults = bind_ok(fixture, "CREATE VINDEX vidx_embedding_default ON users (embedding) USING HNSW;");
    const auto * bound_defaults = static_cast<const BoundCreateVectorIndexStatement *>(defaults.get());
    require(bound_defaults->metric() == VectorDistanceMetric::L2, "CREATE VINDEX default metric mismatch");
    require(bound_defaults->max_neighbors() == 16, "CREATE VINDEX default max_neighbors mismatch");
    require(bound_defaults->ef_construction() == 200, "CREATE VINDEX default ef_construction mismatch");
    require(bound_defaults->ef_search_default() == 64, "CREATE VINDEX default ef_search mismatch");
    require(bound_defaults->random_seed() == 0, "CREATE VINDEX default random_seed mismatch");

    require(bind_error(fixture, "CREATE VINDEX vidx_age ON users (age) USING HNSW;").is(BinderErrorCode::InvalidType), "vector index scalar column error mismatch");
    require(bind_error(fixture, "CREATE VINDEX vidx_missing ON users (missing) USING HNSW;").is(BinderErrorCode::ColumnNotFound), "missing vector index column error mismatch");

    const auto * embedding_column = fixture.catalog.view().find_column(fixture.users_id, "embedding");
    require(embedding_column != nullptr, "embedding column lookup failed");
    auto created_index = fixture.catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = embedding_column->id(),
        .name = "vidx_embedding",
    });
    require(created_index.has_value(), "fixture vector index create failed");

    auto drop = bind_ok(fixture, "DROP VINDEX vidx_embedding ON users;");
    require(drop->kind() == BoundStatementKind::DropVectorIndex, "DROP VINDEX kind mismatch");
    const auto * bound_drop = static_cast<const BoundDropVectorIndexStatement *>(drop.get());
    require(bound_drop->database_id() == fixture.database_id, "DROP VINDEX database id mismatch");
    require(bound_drop->collection_id() == fixture.users_id, "DROP VINDEX collection id mismatch");
    require(bound_drop->collection_name() == "users", "DROP VINDEX collection name mismatch");
    require(bound_drop->index_name() == "vidx_embedding", "DROP VINDEX index name mismatch");
    require(!bound_drop->if_exists(), "DROP VINDEX if-exists mismatch");

    require(bind_error(fixture, "DROP VINDEX missing ON users;").is(BinderErrorCode::IndexNotFound), "missing vector index error mismatch");

    auto drop_missing = bind_ok(fixture, "DROP VINDEX IF EXISTS missing ON users;");
    const auto * bound_drop_missing = static_cast<const BoundDropVectorIndexStatement *>(drop_missing.get());
    require(bound_drop_missing->if_exists(), "DROP VINDEX IF EXISTS mismatch");
}

} // namespace

int main()
{
    try {
        test_use_and_missing_database_context();
        test_select_binding();
        test_select_alias_binding();
        test_select_errors();
        test_function_binding();
        test_insert_binding();
        test_insert_errors();
        test_update_delete_binding();
        test_ddl_and_metadata_binding();
        test_index_binding();
        test_vector_index_binding();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
