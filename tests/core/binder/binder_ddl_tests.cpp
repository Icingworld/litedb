#include "test_support.hpp"

#include <exception>
#include <iostream>

#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"

namespace
{

using namespace litedb::test::binder;
using namespace litedb::core::catalog::entry;

IndexId create_age_index(Fixture & fixture, std::string name = "idx_age")
{
    auto index = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = fixture.age_column_id,
        .index_name = std::move(name),
        .kind = IndexKind::BTree,
    });
    if (!index.has_value()) {
        throw std::runtime_error(index.error().message());
    }
    return *index;
}

VIndexId create_embedding_index(Fixture & fixture, std::string name = "vidx_embedding")
{
    auto index = fixture.catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = fixture.embedding_column_id,
        .vector_index_name = std::move(name),
    });
    if (!index.has_value()) {
        throw std::runtime_error(index.error().message());
    }
    return *index;
}

void test_use_show_and_describe()
{
    Fixture fixture;
    auto use = bind_ok<BoundUseStatement>(fixture, "USE demo;");
    require(use->database_id() == fixture.database_id, "USE database id mismatch");

    bind_ok<BoundShowDatabasesStatement>(fixture, "SHOW DATABASES;");
    auto collections = bind_ok<BoundShowCollectionsStatement>(fixture, "SHOW COLLECTIONS;");
    require(collections->database_id() == fixture.database_id, "SHOW COLLECTIONS database id mismatch");
    auto indexes = bind_ok<BoundShowIndexesStatement>(fixture, "SHOW INDEXES FROM users;");
    require(indexes->collection_id() == fixture.users_id, "SHOW INDEXES collection id mismatch");
    auto vector_indexes = bind_ok<BoundShowVectorIndexesStatement>(fixture, "SHOW VINDEXES FROM users;");
    require(vector_indexes->collection_id() == fixture.users_id, "SHOW VINDEXES collection id mismatch");
    auto describe = bind_ok<BoundDescribeCollectionStatement>(fixture, "DESCRIBE users;");
    require(describe->collection_id() == fixture.users_id, "DESCRIBE collection id mismatch");

    require_error(fixture, "USE missing;", BinderErrorCode::DatabaseNotFound);
    require_error(fixture, "DESCRIBE missing;", BinderErrorCode::CollectionNotFound);
}

void test_create_database_and_collection_intent()
{
    Fixture fixture;
    auto create_database = bind_ok<BoundCreateDatabaseStatement>(
        fixture,
        "CREATE DATABASE analytics;"
    );
    require(create_database->database_name() == "analytics", "CREATE DATABASE intent mismatch");

    auto database_noop = bind_ok<BoundCreateDatabaseStatement>(
        fixture,
        "CREATE DATABASE IF NOT EXISTS demo;"
    );
    require(!database_noop->database_name().has_value(), "CREATE DATABASE no-op mismatch");
    require_error(fixture, "CREATE DATABASE demo;", BinderErrorCode::DatabaseAlreadyExists);

    auto collection = bind_ok<BoundCreateCollectionStatement>(
        fixture,
        "CREATE COLLECTION posts (id BIGINT NOT NULL, tag VARCHAR(32) NULL);"
    );
    require(collection->database_id() == fixture.database_id, "CREATE COLLECTION database mismatch");
    require(collection->collection_name() == "posts", "CREATE COLLECTION intent mismatch");
    require(collection->columns().size() == 2, "CREATE COLLECTION columns mismatch");

    auto collection_noop = bind_ok<BoundCreateCollectionStatement>(
        fixture,
        "CREATE COLLECTION IF NOT EXISTS users (id BIGINT NOT NULL);"
    );
    require(!collection_noop->collection_name().has_value(), "CREATE COLLECTION no-op mismatch");
    auto invalid_collection_noop = bind_ok<BoundCreateCollectionStatement>(
        fixture,
        "CREATE COLLECTION IF NOT EXISTS users (id BIGINT, ID INTEGER);"
    );
    require(
        !invalid_collection_noop->collection_name().has_value(),
        "CREATE COLLECTION invalid payload no-op mismatch"
    );
    require_error(
        fixture,
        "CREATE COLLECTION users (id BIGINT);",
        BinderErrorCode::CollectionAlreadyExists
    );
}

void test_collection_definition_errors()
{
    Fixture fixture;
    require_error(
        fixture,
        "CREATE COLLECTION duplicate_columns (id BIGINT, ID INTEGER);",
        BinderErrorCode::DuplicateColumn
    );
    require_error(
        fixture,
        "CREATE COLLECTION bad_default (age INTEGER DEFAULT 'old');",
        BinderErrorCode::InvalidType
    );
    require_error(
        fixture,
        "CREATE COLLECTION null_default (id BIGINT NOT NULL DEFAULT NULL);",
        BinderErrorCode::NotNullable
    );
    require_error(
        fixture,
        "CREATE COLLECTION bad_varchar (name VARCHAR(0));",
        BinderErrorCode::InvalidType
    );
    require_error(
        fixture,
        "CREATE COLLECTION bad_vector (embedding VECTOR(0));",
        BinderErrorCode::InvalidType
    );
}

void test_scalar_index_intent_and_identity()
{
    Fixture fixture;
    auto create = bind_ok<BoundCreateIndexStatement>(
        fixture,
        "CREATE UNIQUE INDEX idx_name ON users (name) USING BTREE;"
    );
    require(create->column_id() == fixture.name_column_id, "CREATE INDEX column id mismatch");
    require(create->index_name() == "idx_name", "CREATE INDEX intent mismatch");
    require(create->index_kind() == IndexKind::BTree, "CREATE INDEX kind mismatch");
    require(create->unique(), "CREATE UNIQUE INDEX flag missing");

    const auto existing_id = create_age_index(fixture);
    auto create_noop = bind_ok<BoundCreateIndexStatement>(
        fixture,
        "CREATE INDEX IF NOT EXISTS idx_age ON users (age);"
    );
    require(!create_noop->index_name().has_value(), "CREATE INDEX no-op mismatch");
    auto missing_column_noop = bind_ok<BoundCreateIndexStatement>(
        fixture,
        "CREATE INDEX IF NOT EXISTS idx_age ON users (missing);"
    );
    require(!missing_column_noop->index_name().has_value(), "CREATE INDEX invalid payload no-op mismatch");
    require_error(
        fixture,
        "CREATE INDEX idx_age ON users (age);",
        BinderErrorCode::IndexAlreadyExists
    );

    auto drop = bind_ok<BoundDropIndexStatement>(fixture, "DROP INDEX idx_age ON users;");
    require(drop->index_id() == existing_id, "DROP INDEX id mismatch");
    auto drop_noop = bind_ok<BoundDropIndexStatement>(
        fixture,
        "DROP INDEX IF EXISTS missing ON users;"
    );
    require(!drop_noop->index_id().has_value(), "DROP INDEX no-op mismatch");
    require_error(
        fixture,
        "DROP INDEX missing ON users;",
        BinderErrorCode::IndexNotFound
    );

    require_error(
        fixture,
        "CREATE INDEX idx_embedding ON users (embedding);",
        BinderErrorCode::InvalidType
    );
    require_error(
        fixture,
        "CREATE INDEX idx_missing ON users (missing);",
        BinderErrorCode::ColumnNotFound
    );
}

void test_vector_index_intent_options_and_identity()
{
    Fixture fixture;
    auto create = bind_ok<BoundCreateVectorIndexStatement>(
        fixture,
        "CREATE VINDEX vidx_new ON users (embedding) USING HNSW "
        "WITH (metric = INNER_PRODUCT, max_neighbors = 24, "
        "ef_construction = 240, ef_search = 80, random_seed = 9);"
    );
    require(create->column_id() == fixture.embedding_column_id, "CREATE VINDEX column id mismatch");
    require(create->vector_index_name() == "vidx_new", "CREATE VINDEX intent mismatch");
    require(create->vector_index_kind() == VectorIndexKind::Hnsw, "CREATE VINDEX kind mismatch");
    require(create->metric() == VectorDistanceMetric::InnerProduct, "CREATE VINDEX metric mismatch");
    require(create->max_neighbors() == 24, "max_neighbors mismatch");
    require(create->ef_construction() == 240, "ef_construction mismatch");
    require(create->ef_search_default() == 80, "ef_search mismatch");
    require(create->random_seed() == 9, "random seed mismatch");

    const auto existing_id = create_embedding_index(fixture);
    auto create_noop = bind_ok<BoundCreateVectorIndexStatement>(
        fixture,
        "CREATE VINDEX IF NOT EXISTS vidx_embedding ON users (embedding) USING HNSW;"
    );
    require(!create_noop->vector_index_name().has_value(), "CREATE VINDEX no-op mismatch");
    auto invalid_options_noop = bind_ok<BoundCreateVectorIndexStatement>(
        fixture,
        "CREATE VINDEX IF NOT EXISTS vidx_embedding ON users (embedding) USING HNSW "
        "WITH (max_neighbors = 0);"
    );
    require(
        !invalid_options_noop->vector_index_name().has_value(),
        "CREATE VINDEX invalid payload no-op mismatch"
    );

    auto drop = bind_ok<BoundDropVectorIndexStatement>(
        fixture,
        "DROP VINDEX vidx_embedding ON users;"
    );
    require(drop->vector_index_id() == existing_id, "DROP VINDEX id mismatch");
    auto drop_noop = bind_ok<BoundDropVectorIndexStatement>(
        fixture,
        "DROP VINDEX IF EXISTS missing ON users;"
    );
    require(!drop_noop->vector_index_id().has_value(), "DROP VINDEX no-op mismatch");
    require_error(
        fixture,
        "DROP VINDEX missing ON users;",
        BinderErrorCode::VectorIndexNotFound
    );

    require_error(
        fixture,
        "CREATE VINDEX bad_m ON users (embedding) USING HNSW WITH (max_neighbors = 0);",
        BinderErrorCode::InvalidIndexOptions
    );
    require_error(
        fixture,
        "CREATE VINDEX bad_ef ON users (embedding) USING HNSW "
        "WITH (max_neighbors = 32, ef_construction = 16);",
        BinderErrorCode::InvalidIndexOptions
    );
    require_error(
        fixture,
        "CREATE VINDEX bad_search ON users (embedding) USING HNSW WITH (ef_search = 0);",
        BinderErrorCode::InvalidIndexOptions
    );
    require_error(
        fixture,
        "CREATE VINDEX scalar_column ON users (age) USING HNSW;",
        BinderErrorCode::InvalidType
    );
}

void test_drop_database_and_collection_intent()
{
    Fixture fixture;
    auto database = bind_ok<BoundDropDatabaseStatement>(fixture, "DROP DATABASE demo;");
    require(database->database_id() == fixture.database_id, "DROP DATABASE id mismatch");
    auto missing_database = bind_ok<BoundDropDatabaseStatement>(
        fixture,
        "DROP DATABASE IF EXISTS missing;"
    );
    require(!missing_database->database_id().has_value(), "DROP DATABASE no-op mismatch");

    auto collection = bind_ok<BoundDropCollectionStatement>(fixture, "DROP COLLECTION users;");
    require(collection->collection_id() == fixture.users_id, "DROP COLLECTION id mismatch");
    auto missing_collection = bind_ok<BoundDropCollectionStatement>(
        fixture,
        "DROP COLLECTION IF EXISTS missing;"
    );
    require(!missing_collection->collection_id().has_value(), "DROP COLLECTION no-op mismatch");
}

void run_suite()
{
    test_use_show_and_describe();
    test_create_database_and_collection_intent();
    test_collection_definition_errors();
    test_scalar_index_intent_and_identity();
    test_vector_index_intent_options_and_identity();
    test_drop_database_and_collection_intent();
}

} // namespace

int main()
{
    try {
        run_suite();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
