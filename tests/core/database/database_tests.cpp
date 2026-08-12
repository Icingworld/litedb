#include "core/database/database_engine.hpp"
#include "core/database/session.hpp"
#include "core/index/scalar_index_key.hpp"
#include "../storage/temporary_directory.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core::database;
using namespace litedb::core::executor;
using namespace litedb::core::index;
using namespace litedb::core::common;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
const T & get_value(const Value & value)
{
    return std::get<T>(value.data());
}

std::unique_ptr<DatabaseEngine> open_database(DatabaseConfig config)
{
    auto opened = DatabaseEngine::open(std::move(config));
    if (!opened.has_value()) {
        throw std::runtime_error(opened.error().message());
    }
    return std::move(*opened);
}

class TestDatabase
{
public:
    explicit TestDatabase(DatabaseConfig config)
        : engine_(open_database(std::move(config)))
        , session_(*engine_)
    {
    }

    auto execute_sql(std::string_view sql) { return session_.execute_sql(sql); }
    auto current_database_id() const noexcept { return session_.current_database_id(); }
    auto catalog() const noexcept { return engine_->catalog(); }
    const auto & index_engine() const noexcept { return engine_->index_engine(); }

private:
    std::unique_ptr<DatabaseEngine> engine_;
    Session session_;
};

std::vector<litedb::core::common::RecordId> find_index_equal(TestDatabase & engine, litedb::core::common::IndexId index_id, Value value)
{
    auto key = ScalarIndexKey::from_value(std::move(value));
    require(key.has_value(), "index key creation failed");

    auto index_view = engine.index_engine().find_index(index_id);
    require(index_view.has_value(), "managed index missing");

    auto found = engine.index_engine().find_equal(index_id, *key);
    require(found.has_value(), "index lookup failed");
    return std::move(*found);
}

ExecutionResult execute_ok(TestDatabase & engine, std::string_view sql)
{
    auto result = engine.execute_sql(sql);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

SessionError execute_error(TestDatabase & engine, std::string_view sql)
{
    auto result = engine.execute_sql(sql);
    require(!result.has_value(), "SQL should fail");
    return std::move(result.error());
}

void test_execute_sql_end_to_end()
{
    litedb::tests::TemporaryDirectory data_directory {"litedb-engine-end-to-end"};
    TestDatabase engine {DatabaseConfig {.data_dir = data_directory.path()}};

    auto create_database = execute_ok(engine, "CREATE DATABASE demo;");
    require(create_database.kind == ExecutionResultKind::Command, "CREATE DATABASE result kind mismatch");
    require(create_database.affected_rows == 1, "CREATE DATABASE affected rows mismatch");

    auto use_database = execute_ok(engine, "USE demo;");
    require(use_database.kind == ExecutionResultKind::UseDatabase, "USE result kind mismatch");
    require(engine.current_database_id().has_value(), "Database session should select database");
    require(use_database.selected_database_id == engine.current_database_id(), "USE selected database mismatch");

    auto create_collection = execute_ok(
        engine,
        "CREATE COLLECTION users ("
        "id BIGINT, "
        "name VARCHAR(64), "
        "age INTEGER"
        ");"
    );
    require(create_collection.affected_rows == 1, "CREATE COLLECTION affected rows mismatch");

    auto insert = execute_ok(engine, "INSERT INTO users VALUES (1, 'alice', 18);");
    require(insert.affected_rows == 1, "INSERT affected rows mismatch");

    auto create_index = execute_ok(engine, "CREATE INDEX idx_age ON users (age);");
    require(create_index.affected_rows == 1, "CREATE INDEX affected rows mismatch");
    const auto collection = engine.catalog().find_collection(engine.current_database_id().value(), "users");
    require(collection.has_value(), "created collection lookup failed");
    const auto index = engine.catalog().find_index(collection->id(), "idx_age");
    require(index.has_value(), "created index missing");
    const auto index_id = index->id();

    auto selected = execute_ok(engine, "SELECT name, age FROM users WHERE id = 1;");
    require(selected.kind == ExecutionResultKind::RowSet, "SELECT result kind mismatch");
    require(selected.columns.size() == 2, "SELECT column count mismatch");
    require(selected.rows.size() == 1, "SELECT row count mismatch");
    require(get_value<std::string>(selected.rows[0].values[0]) == "alice", "SELECT name mismatch");
    require(get_value<std::int32_t>(selected.rows[0].values[1]) == 18, "SELECT age mismatch");

    auto alias_selected = execute_ok(engine, "SELECT age + 1 AS next_age FROM users ORDER BY next_age ASC;");
    require(alias_selected.columns.size() == 1, "SELECT alias column count mismatch");
    require(alias_selected.columns[0].name == "next_age", "SELECT alias column name mismatch");
    require(get_value<std::int32_t>(alias_selected.rows[0].values[0]) == 19, "SELECT alias value mismatch");
    require(find_index_equal(engine, index_id, Value {std::int32_t {18}}).size() == 1, "CREATE INDEX should build existing data");

    auto update = execute_ok(engine, "UPDATE users SET age = 19 WHERE id = 1;");
    require(update.affected_rows == 1, "indexed UPDATE affected rows mismatch");
    require(find_index_equal(engine, index_id, Value {std::int32_t {18}}).empty(), "UPDATE should remove old index key");
    require(find_index_equal(engine, index_id, Value {std::int32_t {19}}).size() == 1, "UPDATE should add new index key");

    auto delete_result = execute_ok(engine, "DELETE FROM users WHERE id = 1;");
    require(delete_result.affected_rows == 1, "indexed DELETE affected rows mismatch");
    require(find_index_equal(engine, index_id, Value {std::int32_t {19}}).empty(), "DELETE should remove index key");

    auto drop_index = execute_ok(engine, "DROP INDEX idx_age ON users;");
    require(drop_index.affected_rows == 1, "DROP INDEX affected rows mismatch");
    require(!engine.catalog().find_index(collection->id(), "idx_age").has_value(), "dropped index should leave catalog");
    require(!engine.index_engine().find_index(index_id).has_value(), "dropped index should leave engine");
}

void test_column_unique_creates_and_enforces_index()
{
    litedb::tests::TemporaryDirectory data_directory {"litedb-engine-column-unique"};
    TestDatabase engine {DatabaseConfig {.data_dir = data_directory.path()}};
    execute_ok(engine, "CREATE DATABASE constraints;");
    execute_ok(engine, "USE constraints;");
    execute_ok(engine, "CREATE COLLECTION users (id BIGINT UNIQUE, name VARCHAR(64));");

    const auto collection = engine.catalog().find_collection(
        engine.current_database_id().value(),
        "users"
    );
    require(collection.has_value(), "UNIQUE collection lookup failed");
    const auto indexes = engine.catalog().list_indexes(collection->id());
    require(indexes.size() == 1 && indexes.front().get().unique(),
            "UNIQUE column should publish one unique index");
    require(engine.index_engine().find_index(indexes.front().get().id()).has_value(),
            "UNIQUE column index should be loaded into the runtime engine");

    execute_ok(engine, "INSERT INTO users VALUES (1, 'alice');");
    auto duplicate = engine.execute_sql("INSERT INTO users VALUES (1, 'bob');");
    require(!duplicate.has_value(), "duplicate UNIQUE value should be rejected");
    auto rows = execute_ok(engine, "SELECT name FROM users WHERE id = 1;");
    require(rows.rows.size() == 1
                && get_value<std::string>(rows.rows.front().values.front()) == "alice",
            "failed UNIQUE insert must not change stored rows");
}

void test_vector_distance_query()
{
    litedb::tests::TemporaryDirectory data_directory {"litedb-engine-vector-query"};
    TestDatabase engine {DatabaseConfig {.data_dir = data_directory.path()}};
    execute_ok(engine, "CREATE DATABASE vectors;");
    execute_ok(engine, "USE vectors;");
    execute_ok(engine, "CREATE COLLECTION docs (id BIGINT, embedding VECTOR(3));");
    execute_ok(engine, "INSERT INTO docs VALUES (1, [0.0, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (2, [1.0, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (3, [0.2, 0.0, 0.0]);");

    auto result = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 2;"
    );
    require(result.kind == ExecutionResultKind::RowSet, "vector SELECT result kind mismatch");
    require(result.rows.size() == 2, "vector SELECT row count mismatch");
    require(get_value<std::int64_t>(result.rows[0].values[0]) == 1, "vector nearest first mismatch");
    require(get_value<std::int64_t>(result.rows[1].values[0]) == 3, "vector nearest second mismatch");
}

void test_vector_index_ddl()
{
    litedb::tests::TemporaryDirectory data_directory {"litedb-engine-vector-index"};
    TestDatabase engine {DatabaseConfig {.data_dir = data_directory.path()}};
    execute_ok(engine, "CREATE DATABASE vectors;");
    execute_ok(engine, "USE vectors;");
    execute_ok(engine, "CREATE COLLECTION docs (id BIGINT, embedding VECTOR(3));");

    auto created = execute_ok(
        engine,
        "CREATE VINDEX vidx_embedding ON docs (embedding) USING HNSW "
        "WITH (metric = COSINE, max_neighbors = 24, ef_construction = 240, ef_search = 80, random_seed = 7);"
    );
    require(created.affected_rows == 1, "CREATE VINDEX affected rows mismatch");

    const auto collection = engine.catalog().find_collection(engine.current_database_id().value(), "docs");
    require(collection.has_value(), "vector index collection lookup failed");
    const auto index = engine.catalog().find_vector_index(collection->id(), "vidx_embedding");
    require(index.has_value(), "created vector index missing");
    require(index->index_kind() == litedb::core::catalog::entry::VectorIndexKind::Hnsw, "vector index kind mismatch");
    require(index->metric() == litedb::core::catalog::entry::VectorDistanceMetric::Cosine, "vector index metric mismatch");
    require(index->dimension() == 3, "vector index dimension mismatch");
    require(index->max_neighbors() == 24, "vector index max_neighbors mismatch");
    require(index->ef_construction() == 240, "vector index ef_construction mismatch");
    require(index->ef_search_default() == 80, "vector index ef_search mismatch");
    require(index->random_seed() == 7, "vector index random_seed mismatch");

    auto existing = execute_ok(engine, "CREATE VINDEX IF NOT EXISTS vidx_embedding ON docs (embedding) USING HNSW;");
    require(existing.affected_rows == 0, "CREATE VINDEX IF NOT EXISTS affected rows mismatch");

    auto dropped = execute_ok(engine, "DROP VINDEX vidx_embedding ON docs;");
    require(dropped.affected_rows == 1, "DROP VINDEX affected rows mismatch");
    require(!engine.catalog().find_vector_index(collection->id(), "vidx_embedding").has_value(), "dropped vector index should leave catalog");

    auto missing = execute_ok(engine, "DROP VINDEX IF EXISTS vidx_embedding ON docs;");
    require(missing.affected_rows == 0, "DROP VINDEX IF EXISTS affected rows mismatch");
}

void test_vector_index_query_pipeline()
{
    litedb::tests::TemporaryDirectory data_directory {"litedb-engine-vector-pipeline"};
    TestDatabase engine {DatabaseConfig {.data_dir = data_directory.path()}};
    execute_ok(engine, "CREATE DATABASE vectors;");
    execute_ok(engine, "USE vectors;");
    execute_ok(engine, "CREATE COLLECTION docs (id BIGINT, category INTEGER, embedding VECTOR(3));");
    execute_ok(engine, "INSERT INTO docs VALUES (1, 1, [0.0, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (2, 2, [1.0, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (3, 1, [0.2, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (4, 2, [2.0, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (5, 9, NULL);");
    execute_ok(engine, "CREATE VINDEX vidx_l2 ON docs (embedding) USING HNSW WITH (metric = L2);");

    auto offset = execute_ok(
        engine,
        "SELECT id, l2_distance(embedding, [0.1, 0.0, 0.0]) AS distance "
        "FROM docs ORDER BY distance ASC LIMIT 2 OFFSET 1;"
    );
    require(offset.rows.size() == 2, "vector OFFSET row count mismatch");
    require(get_value<std::int64_t>(offset.rows[0].values[0]) == 3, "vector OFFSET first row mismatch");
    require(get_value<std::int64_t>(offset.rows[1].values[0]) == 2, "vector OFFSET second row mismatch");

    auto filtered = execute_ok(
        engine,
        "SELECT id FROM docs WHERE category = 1 "
        "ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 2;"
    );
    require(filtered.rows.size() == 2, "filtered vector TopK row count mismatch");
    require(get_value<std::int64_t>(filtered.rows[0].values[0]) == 1, "filtered vector first row mismatch");
    require(get_value<std::int64_t>(filtered.rows[1].values[0]) == 3, "filtered vector second row mismatch");

    execute_ok(engine, "INSERT INTO docs VALUES (7, 9, [0.3, 0.0, 0.0]);");
    auto filtered_fallback = execute_ok(
        engine,
        "SELECT id FROM docs WHERE category = 9 "
        "ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 2;"
    );
    require(filtered_fallback.rows.size() == 2, "filtered fallback should preserve all result rows");
    require(get_value<std::int64_t>(filtered_fallback.rows[0].values[0]) == 7,
            "filtered fallback must retain distance ordering");
    require(get_value<std::int64_t>(filtered_fallback.rows[1].values[0]) == 5,
            "filtered fallback must place NULL distance last");

    auto short_index_fallback = execute_ok(
        engine,
        "SELECT id FROM docs "
        "ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 3 OFFSET 3;"
    );
    require(short_index_fallback.rows.size() == 3,
            "short-index fallback OFFSET row count mismatch");
    require(get_value<std::int64_t>(short_index_fallback.rows[0].values[0]) == 2,
            "short-index fallback first row mismatch");
    require(get_value<std::int64_t>(short_index_fallback.rows[1].values[0]) == 4,
            "short-index fallback second row mismatch");
    require(get_value<std::int64_t>(short_index_fallback.rows[2].values[0]) == 5,
            "short-index fallback must place NULL distance last");

    execute_ok(engine, "INSERT INTO docs VALUES (6, 1, [0.1, 0.0, 0.0]);");
    auto after_insert = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 1;"
    );
    require(get_value<std::int64_t>(after_insert.rows[0].values[0]) == 6, "vector index INSERT sync mismatch");

    execute_ok(engine, "UPDATE docs SET embedding = [4.0, 0.0, 0.0] WHERE id = 6;");
    auto after_update = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 1;"
    );
    require(get_value<std::int64_t>(after_update.rows[0].values[0]) == 1, "vector index UPDATE sync mismatch");

    execute_ok(engine, "DELETE FROM docs WHERE id = 1;");
    auto after_delete = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 1;"
    );
    require(get_value<std::int64_t>(after_delete.rows[0].values[0]) == 3, "vector index DELETE sync mismatch");

    execute_ok(engine, "DROP VINDEX vidx_l2 ON docs;");
    auto after_drop = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 1;"
    );
    require(get_value<std::int64_t>(after_drop.rows[0].values[0]) == 3, "SeqScan result after DROP VINDEX mismatch");

    execute_ok(engine, "CREATE VINDEX vidx_cos ON docs (embedding) USING HNSW WITH (metric = COSINE);");
    auto cosine = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY cosine_distance(embedding, [1.0, 0.0, 0.0]) ASC LIMIT 1;"
    );
    require(cosine.rows.size() == 1, "cosine vector pipeline should return one row");

    execute_ok(engine, "CREATE VINDEX vidx_ip ON docs (embedding) USING HNSW WITH (metric = INNER_PRODUCT);");
    auto inner_product = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY inner_product(embedding, [1.0, 0.0, 0.0]) DESC LIMIT 1;"
    );
    require(get_value<std::int64_t>(inner_product.rows[0].values[0]) == 6, "inner product DESC pipeline mismatch");
}

void test_engine_error_mapping()
{
    litedb::tests::TemporaryDirectory data_directory {"litedb-engine-errors"};
    TestDatabase engine {DatabaseConfig {.data_dir = data_directory.path()}};

    auto parse_error = execute_error(engine, "SELECT FROM;");
    require(parse_error.is(SessionErrorCode::ParserError), "parser error code mismatch");

    auto binder_error = execute_error(engine, "SHOW COLLECTIONS;");
    require(binder_error.is(SessionErrorCode::BinderError), "binder error code mismatch");
}

void test_sessions_share_instance_but_keep_context()
{
    litedb::tests::TemporaryDirectory data_directory {"litedb-engine-sessions"};
    auto engine = open_database(DatabaseConfig {.data_dir = data_directory.path()});
    Session first {*engine};
    Session second {*engine};

    auto create_database = first.execute_sql("CREATE DATABASE shared;");
    require(create_database.has_value(), "CREATE DATABASE should succeed");

    auto first_use = first.execute_sql("USE shared;");
    require(first_use.has_value(), "first USE should succeed");
    require(first.current_database_id().has_value(), "first session should select database");
    require(!second.current_database_id().has_value(), "second session should not inherit selected database");

    auto second_use = second.execute_sql("USE shared;");
    require(second_use.has_value(), "second USE should see shared database");
    require(second.current_database_id() == first.current_database_id(), "sessions should select same database id");
}

} // namespace

int main()
{
    try {
        test_execute_sql_end_to_end();
        test_column_unique_creates_and_enforces_index();
        test_vector_distance_query();
        test_vector_index_ddl();
        test_vector_index_query_pipeline();
        test_engine_error_mapping();
        test_sessions_share_instance_but_keep_context();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
