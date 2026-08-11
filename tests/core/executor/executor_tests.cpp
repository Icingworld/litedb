#include "core/executor/executor.hpp"
#include "core/binder/binder.hpp"
#include "core/binder/binder_context.hpp"
#include "core/binder/bound/bound_assignment.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/session_context.hpp"
#include "core/function/builtin/builtin_functions.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/index/index_engine.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/logical_planner/logical_planner.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/optimizer/optimizer.hpp"
#include "core/parser/parser.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/command/use_plan.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"
#include "core/wal/wal_manager.hpp"
#include "core/meta/meta_store.hpp"
#include "../storage/temporary_directory.hpp"

#include <exception>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

template <typename T>
const T & value_as(const common::Value & value)
{
    return std::get<T>(value.data());
}

struct Fixture
{
    litedb::tests::TemporaryDirectory directory {"litedb-executor-tests"};
    filesystem::FileSystem filesystem {filesystem::create_platform_filesystem()};
    meta::CatalogEditor catalog;
    meta::CatalogPublisher publisher {directory.path() / "meta.ldb", filesystem};
    storage::StorageEngine storage {
        directory.path(),
        filesystem,
        storage::StorageOpenMode::TransactionalStaging,
    };
    index::IndexEngine index_engine {directory.path(), filesystem};
    vindex::VectorIndexEngine vector_index_engine {directory.path() / "vindexes", filesystem};
    std::optional<wal::WalManager> wal;
    std::unique_ptr<transaction::TransactionManager> transaction_manager;
    common::DatabaseId database_id {0};
    common::CollectionId collection_id {0};

    Fixture()
    {
        auto database = catalog.create_database(meta::CreateDatabaseRequest {.name = "demo"});
        require(database.has_value(), "executor fixture database creation failed");
        database_id = *database;
        auto collection = catalog.create_collection(meta::CreateCollectionRequest {
            .database_id = database_id,
            .name = "users",
            .columns = {
                meta::ColumnDefinition {.name = "id", .type = common::LogicalType {common::LogicalTypeId::BigInt, std::nullopt}},
                meta::ColumnDefinition {.name = "age", .type = common::LogicalType {common::LogicalTypeId::Integer, std::nullopt}},
                meta::ColumnDefinition {.name = "embedding", .type = common::LogicalType {common::LogicalTypeId::Vector, 3}},
            },
        });
        require(collection.has_value(), "executor fixture collection creation failed");
        collection_id = *collection;

        auto schema = storage::load_collection_schema(catalog.view(), collection_id);
        require(schema.has_value(), "executor fixture schema load failed");
        require(storage.create_collection(std::move(*schema)).has_value(), "executor fixture storage creation failed");

        auto opened_wal = wal::WalManager::open(directory.path() / "wal", filesystem);
        require(opened_wal.has_value(), "executor fixture WAL creation failed");
        wal = std::move(*opened_wal);
        require(publisher.open_or_initialize().has_value(), "executor fixture publisher open failed");
        require(publisher.publish_committed(catalog.snapshot()).has_value(), "executor fixture catalog publish failed");
        transaction_manager = std::make_unique<transaction::TransactionManager>(
            directory.path(),
            filesystem,
            publisher,
            storage,
            index_engine,
            vector_index_engine,
            *wal,
            0
        );
    }
};

std::unique_ptr<parser::ast::StatementNode> parse_ok(std::string_view sql)
{
    parser::Parser parser {std::string(sql)};
    auto parsed = parser.parse();
    if (!parsed.has_value()) {
        throw std::runtime_error(parsed.error().message());
    }
    return std::move(*parsed);
}

std::unique_ptr<physical_planner::plan::PhysicalPlan> plan_ok(
    Fixture & fixture,
    std::string_view sql
)
{
    auto statement = parse_ok(sql);
    binder::SessionContext session {.current_database_id = fixture.database_id};
    binder::BinderContext context {
        fixture.catalog.view(),
        session,
        function::builtin::builtin_function_catalog(),
    };
    binder::Binder binder {context};
    auto bound = binder.bind(*statement);
    if (!bound.has_value()) {
        throw std::runtime_error(bound.error().message());
    }
    logical_planner::LogicalPlanner logical_planner;
    auto logical = logical_planner.plan(std::move(*bound));
    optimizer::Optimizer optimizer;
    auto optimized = optimizer.optimize(std::move(logical));
    physical_planner::PhysicalPlanner physical_planner {fixture.catalog.view()};
    return physical_planner.plan(std::move(optimized));
}

executor::ExecutionResult execute_ok(Fixture & fixture, std::string_view sql)
{
    auto plan = plan_ok(fixture, sql);
    executor::Executor executor {
        executor::ExecutionContext {
            .catalog = fixture.catalog.view(),
            .storage = fixture.storage,
            .index_engine = fixture.index_engine,
            .vector_index_engine = fixture.vector_index_engine,
            .transaction_manager = *fixture.transaction_manager,
        },
    };
    auto result = executor.execute(*plan);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

executor::ExecutionError execute_error(Fixture & fixture, std::string_view sql)
{
    auto plan = plan_ok(fixture, sql);
    executor::Executor executor {
        executor::ExecutionContext {
            .catalog = fixture.catalog.view(),
            .storage = fixture.storage,
            .index_engine = fixture.index_engine,
            .vector_index_engine = fixture.vector_index_engine,
            .transaction_manager = *fixture.transaction_manager,
        },
    };
    auto result = executor.execute(*plan);
    require(!result.has_value(), "statement should fail to execute");
    return std::move(result.error());
}

common::IndexId create_age_index(Fixture & fixture, std::string name)
{
    const auto * age = fixture.catalog.view().find_column(fixture.collection_id, "age");
    require(age != nullptr, "executor fixture age column missing");
    auto created = fixture.catalog.create_index(meta::CreateIndexRequest {
        .collection_id = fixture.collection_id,
        .column_ids = {age->id()},
        .name = std::move(name),
    });
    require(created.has_value(), "executor fixture index metadata creation failed");
    require(fixture.publisher.publish_committed(fixture.catalog.snapshot()).has_value(),
            "executor fixture index catalog publish failed");
    const auto * entry = fixture.catalog.view().find_index(*created);
    require(entry != nullptr, "executor fixture index entry missing");
    auto schema = storage::load_collection_schema(fixture.catalog.view(), fixture.collection_id);
    require(schema.has_value(), "executor fixture index schema load failed");
    require(fixture.index_engine.create_index(*entry, *schema, fixture.storage).has_value(),
            "executor fixture runtime index creation failed");
    return *created;
}

common::VIndexId create_embedding_index(Fixture & fixture)
{
    const auto * embedding = fixture.catalog.view().find_column(fixture.collection_id, "embedding");
    require(embedding != nullptr, "executor fixture embedding column missing");
    auto created = fixture.catalog.create_vector_index(meta::CreateVectorIndexRequest {
        .collection_id = fixture.collection_id,
        .column_id = embedding->id(),
        .name = "embedding_l2",
        .metric = meta::entry::VectorDistanceMetric::L2,
        .hnsw_options = meta::entry::HnswOptions {
            .max_neighbors = 16,
            .ef_construction = 32,
            .ef_search_default = 16,
            .random_seed = 1,
        },
    });
    require(created.has_value(), "executor fixture vector index metadata creation failed");
    require(fixture.publisher.publish_committed(fixture.catalog.snapshot()).has_value(),
            "executor fixture vector index catalog publish failed");
    const auto * entry = fixture.catalog.view().find_vector_index(*created);
    require(entry != nullptr, "executor fixture vector index entry missing");
    auto schema = storage::load_collection_schema(fixture.catalog.view(), fixture.collection_id);
    require(schema.has_value(), "executor fixture vector index schema load failed");
    require(fixture.vector_index_engine.create_index(*entry, *schema, fixture.storage).has_value(),
            "executor fixture runtime vector index creation failed");
    return *created;
}

void insert_user(Fixture & fixture, std::int64_t id, std::int32_t age)
{
    auto result = execute_ok(
        fixture,
        "INSERT INTO users VALUES ("
            + std::to_string(id)
            + ", "
            + std::to_string(age)
            + ", [0.1, 0.2, 0.3]);"
    );
    require(result.kind == executor::ExecutionResultKind::Command && result.affected_rows == 1,
            "executor fixture INSERT mismatch");
}

void test_invalid_use()
{
    Fixture fixture;
    executor::Executor executor {
        executor::ExecutionContext {
            .catalog = fixture.catalog.view(),
            .storage = fixture.storage,
            .index_engine = fixture.index_engine,
            .vector_index_engine = fixture.vector_index_engine,
            .transaction_manager = *fixture.transaction_manager,
        },
    };

    physical_planner::plan::UsePlan use {99};
    auto use_result = executor.execute(use);
    require(!use_result.has_value(), "USE of missing database should fail");
    require(use_result.error().is(executor::ExecutionErrorCode::InvalidPlan), "USE error code mismatch");

}

void test_command_metadata_reads()
{
    Fixture fixture;
    auto databases = execute_ok(fixture, "SHOW DATABASES;");
    require(databases.kind == executor::ExecutionResultKind::RowSet,
            "SHOW DATABASES result kind mismatch");
    require(!databases.rows.empty(), "SHOW DATABASES should list the fixture database");

    auto collections = execute_ok(fixture, "SHOW COLLECTIONS;");
    require(collections.kind == executor::ExecutionResultKind::RowSet,
            "SHOW COLLECTIONS result kind mismatch");
    require(!collections.rows.empty(), "SHOW COLLECTIONS should list the fixture collection");

    auto description = execute_ok(fixture, "DESCRIBE users;");
    require(description.kind == executor::ExecutionResultKind::RowSet,
            "DESCRIBE result kind mismatch");
    require(description.rows.size() == 3, "DESCRIBE should return all fixture columns");
}

void test_missing_scan()
{
    Fixture fixture;
    executor::Executor executor {
        executor::ExecutionContext {
            .catalog = fixture.catalog.view(),
            .storage = fixture.storage,
            .index_engine = fixture.index_engine,
            .vector_index_engine = fixture.vector_index_engine,
            .transaction_manager = *fixture.transaction_manager,
        },
    };
    auto query = std::make_unique<physical_planner::plan::QueryPlan>(
        std::make_unique<physical_planner::op::SeqScanOperator>(42)
    );
    auto query_result = executor.execute(*query);
    require(!query_result.has_value(), "scan of missing collection should fail");
    require(query_result.error().is(executor::ExecutionErrorCode::SchemaError), "scan error code mismatch");
}

void test_dml_sort_and_scalar_index_execution()
{
    Fixture fixture;
    (void) create_age_index(fixture, "age_index");
    insert_user(fixture, 1, 18);
    insert_user(fixture, 2, 20);
    insert_user(fixture, 3, 15);

    auto selected = execute_ok(fixture, "SELECT id FROM users WHERE age >= 18 ORDER BY age DESC LIMIT 2;");
    require(selected.rows.size() == 2, "executor indexed SELECT row count mismatch");
    require(value_as<std::int64_t>(selected.rows[0].values[0]) == 2,
            "executor indexed SELECT sort mismatch");
    require(value_as<std::int64_t>(selected.rows[1].values[0]) == 1,
            "executor indexed SELECT second row mismatch");

    auto updated = execute_ok(fixture, "UPDATE users SET age = age + 1 WHERE age >= 20;");
    require(updated.affected_rows == 1, "executor indexed UPDATE affected rows mismatch");
    auto deleted = execute_ok(fixture, "DELETE FROM users WHERE age < 18;");
    require(deleted.affected_rows == 1, "executor indexed DELETE affected rows mismatch");
}

void test_mutation_error_aborts_and_releases_writer()
{
    Fixture fixture;
    insert_user(fixture, 1, 18);

    const auto * age = fixture.catalog.view().find_column(fixture.collection_id, "age");
    require(age != nullptr, "executor mutation error test age column missing");
    std::vector<binder::bound::BoundAssignment> assignments;
    assignments.push_back(binder::bound::BoundAssignment {
        .column_id = age->id(),
        .value = std::make_unique<binder::bound::BoundColumnRefExpression>(
            age->id(),
            999,
            common::LogicalType {common::LogicalTypeId::Integer, std::nullopt}
        ),
    });
    physical_planner::plan::UpdatePlan update {
        fixture.collection_id,
        std::move(assignments),
        std::make_unique<physical_planner::op::SeqScanOperator>(fixture.collection_id),
    };

    executor::Executor executor {
        executor::ExecutionContext {
            .catalog = fixture.catalog.view(),
            .storage = fixture.storage,
            .index_engine = fixture.index_engine,
            .vector_index_engine = fixture.vector_index_engine,
            .transaction_manager = *fixture.transaction_manager,
        },
    };
    auto result = executor.execute(update);
    require(!result.has_value(), "invalid assignment should fail inside mutation transaction");
    require(result.error().is(executor::ExecutionErrorCode::EvaluationError),
            "mutation evaluation error code mismatch");

    auto selected = execute_ok(fixture, "SELECT age FROM users;");
    require(selected.rows.size() == 1, "aborted update should retain the source row");
    require(value_as<std::int32_t>(selected.rows[0].values[0]) == 18,
            "aborted update should not modify the source row");

    auto transaction = fixture.transaction_manager->begin_implicit();
    require(transaction.has_value(), "writer lock should be released after mutation abort");
    require(fixture.transaction_manager->abort(*transaction).has_value(),
            "writer lock probe abort failed");
}

void test_vector_search_execution()
{
    Fixture fixture;
    (void) create_embedding_index(fixture);
    insert_user(fixture, 1, 18);
    insert_user(fixture, 2, 20);
    insert_user(fixture, 3, 15);

    auto selected = execute_ok(
        fixture,
        "SELECT id FROM users "
        "ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC LIMIT 2;"
    );
    require(selected.rows.size() == 2, "executor vector SELECT row count mismatch");
    require(value_as<std::int64_t>(selected.rows[0].values[0]) == 1,
            "executor vector nearest row mismatch");
}

void test_index_descriptor_mismatch_is_invalid_plan()
{
    Fixture fixture;
    const auto index_id = create_age_index(fixture, "age_index");
    auto key = index::ScalarIndexKey::from_value(common::Value {std::int32_t {18}});
    require(key.has_value(), "executor mismatch index key creation failed");
    physical_planner::op::IndexLookup lookup {
        .kind = physical_planner::op::IndexLookupKind::Equal,
        .lower = physical_planner::op::IndexBound {.key = std::move(*key), .inclusive = true},
    };
    auto query = std::make_unique<physical_planner::plan::QueryPlan>(
        std::make_unique<physical_planner::op::IndexScanOperator>(
            fixture.collection_id + 100,
            index_id,
            std::move(lookup)
        )
    );
    executor::Executor executor {
        executor::ExecutionContext {
            .catalog = fixture.catalog.view(),
            .storage = fixture.storage,
            .index_engine = fixture.index_engine,
            .vector_index_engine = fixture.vector_index_engine,
            .transaction_manager = *fixture.transaction_manager,
        },
    };
    auto result = executor.execute(*query);
    require(!result.has_value(), "mismatched physical index descriptor should fail");
    require(result.error().is(executor::ExecutionErrorCode::InvalidPlan),
            "mismatched physical index descriptor error code mismatch");
}

} // namespace

int main()
{
    try {
#if defined(LITEDB_EXECUTOR_COMMAND_TESTS)
        test_invalid_use();
        test_command_metadata_reads();
#elif defined(LITEDB_EXECUTOR_OPERATOR_TESTS)
        test_missing_scan();
        test_index_descriptor_mismatch_is_invalid_plan();
#elif defined(LITEDB_EXECUTOR_QUERY_TESTS)
        test_vector_search_execution();
#elif defined(LITEDB_EXECUTOR_MUTATION_TESTS)
        test_dml_sort_and_scalar_index_execution();
        test_mutation_error_aborts_and_releases_writer();
#else
        test_invalid_use();
        test_command_metadata_reads();
        test_missing_scan();
        test_dml_sort_and_scalar_index_execution();
        test_mutation_error_aborts_and_releases_writer();
        test_vector_search_execution();
        test_index_descriptor_mismatch_is_invalid_plan();
#endif
    } catch (const std::exception & error) {
        std::cerr << "executor_tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "executor_tests passed\n";
    return 0;
}
