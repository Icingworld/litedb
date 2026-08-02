#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/meta/meta.hpp"
#include "core/common/logical_type.hpp"
#include "core/logical_planner/node/logical_filter.hpp"
#include "core/logical_planner/node/logical_limit.hpp"
#include "core/logical_planner/node/logical_order_by.hpp"
#include "core/logical_planner/node/logical_projection.hpp"
#include "core/logical_planner/node/logical_scan.hpp"
#include "core/logical_planner/plan/command/create_collection_plan.hpp"
#include "core/logical_planner/plan/command/create_database_plan.hpp"
#include "core/logical_planner/plan/command/create_index_plan.hpp"
#include "core/logical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/describe_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_database_plan.hpp"
#include "core/logical_planner/plan/command/drop_index_plan.hpp"
#include "core/logical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/show_collections_plan.hpp"
#include "core/logical_planner/plan/command/show_databases_plan.hpp"
#include "core/logical_planner/plan/command/show_indexes_plan.hpp"
#include "core/logical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/logical_planner/plan/command/use_plan.hpp"
#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/node/physical_filter.hpp"
#include "core/physical_planner/node/physical_index_scan.hpp"
#include "core/physical_planner/node/physical_limit.hpp"
#include "core/physical_planner/node/physical_projection.hpp"
#include "core/physical_planner/node/physical_seq_scan.hpp"
#include "core/physical_planner/node/physical_sort.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/physical_planner/statement/physical_command_plan.hpp"
#include "core/physical_planner/statement/physical_insert_plan.hpp"
#include "core/physical_planner/statement/physical_query_plan.hpp"
#include "core/physical_planner/statement/physical_row_mutation_plan.hpp"
#include "core/physical_planner/statement/physical_statement_plan.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;
using namespace litedb::core::meta;
using namespace litedb::core::meta::entry;
using namespace litedb::core::common;
using namespace litedb::core::parser::ast;
using namespace litedb::core::physical_plan;
using namespace litedb::core::planner::logical;
using namespace litedb::core::logical_planner::plan;

constexpr AstNodeLocation loc {1, 1};

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LogicalType type(LogicalTypeId id)
{
    return LogicalType {.id = id, .parameter = std::nullopt};
}

std::unique_ptr<BoundLiteralExpression> literal(LogicalTypeId id, std::string value)
{
    return std::make_unique<BoundLiteralExpression>(type(id), std::move(value), loc);
}

void test_lower_unary_chain()
{
    std::vector<BoundProjectionItem> projections;
    projections.push_back(BoundProjectionItem {
        .expression = literal(LogicalTypeId::BigInt, "1"),
        .alias = "id",
    });

    std::vector<BoundOrderByItem> order_by;
    order_by.push_back(BoundOrderByItem {
        .expression = literal(LogicalTypeId::BigInt, "1"),
        .ascending = false,
    });

    auto logical = std::make_unique<LogicalLimit>(
        std::make_unique<LogicalOrderBy>(
            std::make_unique<LogicalProjection>(
                std::make_unique<LogicalFilter>(
                    std::make_unique<LogicalScan>(DatabaseId {1}, CollectionId {2}, "users", loc),
                    literal(LogicalTypeId::Boolean, "true"),
                    loc
                ),
                std::move(projections),
                loc
            ),
            std::move(order_by),
            loc
        ),
        10,
        5,
        loc
    );

    PhysicalPlanner planner;
    auto physical = planner.plan(*logical);
    require(physical->kind() == PhysicalPlanNodeKind::Limit, "root should lower to physical limit");

    const auto & limit = static_cast<const PhysicalLimit &>(*physical);
    require(limit.limit().value() == 10, "limit value mismatch");
    require(limit.offset().value() == 5, "offset value mismatch");
    require(limit.child().kind() == PhysicalPlanNodeKind::Sort, "order by should lower to sort");

    const auto & sort = static_cast<const PhysicalSort &>(limit.child());
    require(sort.order_by().size() == 1, "sort key count mismatch");
    require(!sort.order_by()[0].ascending, "sort direction mismatch");
    require(sort.child().kind() == PhysicalPlanNodeKind::Projection, "projection should be below sort");

    const auto & projection = static_cast<const PhysicalProjection &>(sort.child());
    require(projection.projections().size() == 1, "projection count mismatch");
    require(projection.child().kind() == PhysicalPlanNodeKind::Filter, "filter should be below projection");

    const auto & filter = static_cast<const PhysicalFilter &>(projection.child());
    require(filter.predicate().type().id == LogicalTypeId::Boolean, "filter predicate type mismatch");
    require(filter.child().kind() == PhysicalPlanNodeKind::SeqScan, "scan should lower to seq scan");

    const auto & scan = static_cast<const PhysicalSeqScan &>(filter.child());
    require(scan.database_id() == DatabaseId {1}, "seq scan database id mismatch");
    require(scan.collection_id() == CollectionId {2}, "seq scan collection id mismatch");
    require(scan.collection_name() == "users", "seq scan collection name mismatch");
}

void test_lower_index_scan()
{
    LogicalScan logical {
        DatabaseId {1},
        CollectionId {2},
        "users",
        LogicalScanIndexHint {
            .index_id = IndexId {3},
            .index_name = "idx_age",
            .index_kind = IndexKind::BTree,
            .column_id = ColumnId {4},
            .column_name = "age",
            .lookup = LogicalIndexLookup {.kind = LogicalIndexLookupKind::Range},
        },
        loc,
    };

    PhysicalPlanner planner;
    auto physical = planner.plan(logical);
    require(physical->kind() == PhysicalPlanNodeKind::IndexScan, "index scan should lower to physical index scan");

    const auto & scan = static_cast<const PhysicalIndexScan &>(*physical);
    require(scan.database_id() == DatabaseId {1}, "index scan database id mismatch");
    require(scan.collection_id() == CollectionId {2}, "index scan collection id mismatch");
    require(scan.collection_name() == "users", "index scan collection name mismatch");
    require(scan.index_id() == IndexId {3}, "index scan id mismatch");
    require(scan.index_name() == "idx_age", "index scan name mismatch");
    require(scan.index_kind() == IndexKind::BTree, "index kind mismatch");
    require(scan.column_id() == ColumnId {4}, "index scan column id mismatch");
    require(scan.column_name() == "age", "index scan column name mismatch");
    require(scan.lookup().kind == PhysicalIndexLookupKind::Range, "index lookup kind mismatch");
}

void test_lower_query_statement()
{
    QueryPlan logical {
        std::make_unique<LogicalScan>(DatabaseId {1}, CollectionId {2}, "users", loc),
        loc,
    };

    PhysicalPlanner planner;
    auto physical = planner.plan(logical);
    require(physical->kind() == PhysicalStatementPlanKind::Query, "query statement kind mismatch");

    const auto & query = static_cast<const PhysicalQueryPlan &>(*physical);
    require(query.root().kind() == PhysicalPlanNodeKind::SeqScan, "query root should lower to seq scan");
}

void test_lower_simple_statement()
{
    PhysicalPlanner planner;

    UsePlan use {DatabaseId {7}, "demo", loc};
    auto physical_use = planner.plan(use);
    require(physical_use->kind() == PhysicalStatementPlanKind::Use, "USE statement kind mismatch");
    const auto & lowered_use = static_cast<const PhysicalUsePlan &>(*physical_use);
    require(lowered_use.database_id() == DatabaseId {7}, "USE database id mismatch");
    require(lowered_use.database_name() == "demo", "USE database name mismatch");

    CreateDatabasePlan create_database {"demo", true, loc};
    auto physical_create_database = planner.plan(create_database);
    require(
        physical_create_database->kind() == PhysicalStatementPlanKind::CreateDatabase,
        "CREATE DATABASE statement kind mismatch"
    );
    const auto & lowered_create_database = static_cast<const PhysicalCreateDatabasePlan &>(*physical_create_database);
    require(lowered_create_database.database_name() == "demo", "CREATE DATABASE name mismatch");
    require(lowered_create_database.if_not_exists(), "CREATE DATABASE if_not_exists mismatch");

    std::vector<ColumnDefinition> columns {
        ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt), .nullable = false},
    };
    CreateCollectionPlan create_collection {DatabaseId {7}, "users", true, columns, std::string {"people"}, loc};
    auto physical_create_collection = planner.plan(create_collection);
    require(
        physical_create_collection->kind() == PhysicalStatementPlanKind::CreateCollection,
        "CREATE COLLECTION statement kind mismatch"
    );
    const auto & lowered_create_collection = static_cast<const PhysicalCreateCollectionPlan &>(*physical_create_collection);
    require(lowered_create_collection.database_id() == DatabaseId {7}, "CREATE COLLECTION database id mismatch");
    require(lowered_create_collection.collection_name() == "users", "CREATE COLLECTION name mismatch");
    require(lowered_create_collection.columns().size() == 1, "CREATE COLLECTION columns mismatch");
    require(lowered_create_collection.comment().value() == "people", "CREATE COLLECTION comment mismatch");

    CreateIndexPlan create_index {
        DatabaseId {7},
        CollectionId {8},
        "users",
        ColumnId {9},
        "age",
        "idx_age",
        IndexKind::BTree,
        false,
        true,
        loc,
    };
    auto physical_create_index = planner.plan(create_index);
    require(physical_create_index->kind() == PhysicalStatementPlanKind::CreateIndex, "CREATE INDEX kind mismatch");
    const auto & lowered_create_index = static_cast<const PhysicalCreateIndexPlan &>(*physical_create_index);
    require(lowered_create_index.collection_id() == CollectionId {8}, "CREATE INDEX collection id mismatch");
    require(lowered_create_index.column_id() == ColumnId {9}, "CREATE INDEX column id mismatch");
    require(lowered_create_index.index_name() == "idx_age", "CREATE INDEX name mismatch");

    CreateVectorIndexPlan create_vindex {
        DatabaseId {7},
        CollectionId {8},
        "users",
        ColumnId {9},
        "embedding",
        "vidx_embedding",
        VectorIndexKind::Hnsw,
        VectorDistanceMetric::Cosine,
        16,
        64,
        32,
        11,
        true,
        loc,
    };
    auto physical_create_vindex = planner.plan(create_vindex);
    require(
        physical_create_vindex->kind() == PhysicalStatementPlanKind::CreateVectorIndex,
        "CREATE VINDEX kind mismatch"
    );
    const auto & lowered_create_vindex = static_cast<const PhysicalCreateVectorIndexPlan &>(*physical_create_vindex);
    require(lowered_create_vindex.index_name() == "vidx_embedding", "CREATE VINDEX name mismatch");
    require(lowered_create_vindex.metric() == VectorDistanceMetric::Cosine, "CREATE VINDEX metric mismatch");

    DropDatabasePlan drop_database {DatabaseId {7}, "demo", true, loc};
    auto physical_drop_database = planner.plan(drop_database);
    require(physical_drop_database->kind() == PhysicalStatementPlanKind::DropDatabase, "DROP DATABASE kind mismatch");
    const auto & lowered_drop_database = static_cast<const PhysicalDropDatabasePlan &>(*physical_drop_database);
    require(lowered_drop_database.database_id().value() == DatabaseId {7}, "DROP DATABASE id mismatch");

    DropCollectionPlan drop_collection {DatabaseId {7}, CollectionId {8}, "users", true, loc};
    auto physical_drop_collection = planner.plan(drop_collection);
    require(physical_drop_collection->kind() == PhysicalStatementPlanKind::DropCollection, "DROP COLLECTION kind mismatch");
    const auto & lowered_drop_collection = static_cast<const PhysicalDropCollectionPlan &>(*physical_drop_collection);
    require(lowered_drop_collection.collection_id().value() == CollectionId {8}, "DROP COLLECTION id mismatch");

    DropIndexPlan drop_index {DatabaseId {7}, CollectionId {8}, "users", "idx_age", true, loc};
    auto physical_drop_index = planner.plan(drop_index);
    require(physical_drop_index->kind() == PhysicalStatementPlanKind::DropIndex, "DROP INDEX kind mismatch");
    const auto & lowered_drop_index = static_cast<const PhysicalDropIndexPlan &>(*physical_drop_index);
    require(lowered_drop_index.index_name() == "idx_age", "DROP INDEX name mismatch");

    DropVectorIndexPlan drop_vindex {DatabaseId {7}, CollectionId {8}, "users", "vidx_embedding", true, loc};
    auto physical_drop_vindex = planner.plan(drop_vindex);
    require(physical_drop_vindex->kind() == PhysicalStatementPlanKind::DropVectorIndex, "DROP VINDEX kind mismatch");
    const auto & lowered_drop_vindex = static_cast<const PhysicalDropVectorIndexPlan &>(*physical_drop_vindex);
    require(lowered_drop_vindex.index_name() == "vidx_embedding", "DROP VINDEX name mismatch");

    ShowDatabasesPlan show_databases {loc};
    auto physical_show_databases = planner.plan(show_databases);
    require(physical_show_databases->kind() == PhysicalStatementPlanKind::ShowDatabases, "SHOW DATABASES kind mismatch");

    ShowCollectionsPlan show_collections {DatabaseId {7}, loc};
    auto physical_show_collections = planner.plan(show_collections);
    require(physical_show_collections->kind() == PhysicalStatementPlanKind::ShowCollections, "SHOW COLLECTIONS kind mismatch");
    require(
        static_cast<const PhysicalShowCollectionsPlan &>(*physical_show_collections).database_id() == DatabaseId {7},
        "SHOW COLLECTIONS database id mismatch"
    );

    ShowIndexesPlan show_indexes {DatabaseId {7}, CollectionId {8}, "users", loc};
    auto physical_show_indexes = planner.plan(show_indexes);
    require(physical_show_indexes->kind() == PhysicalStatementPlanKind::ShowIndexes, "SHOW INDEXES kind mismatch");
    require(
        static_cast<const PhysicalShowIndexesPlan &>(*physical_show_indexes).collection_id() == CollectionId {8},
        "SHOW INDEXES collection id mismatch"
    );

    ShowVectorIndexesPlan show_vindexes {DatabaseId {7}, CollectionId {8}, "users", loc};
    auto physical_show_vindexes = planner.plan(show_vindexes);
    require(physical_show_vindexes->kind() == PhysicalStatementPlanKind::ShowVectorIndexes, "SHOW VINDEXES kind mismatch");
    require(
        static_cast<const PhysicalShowVectorIndexesPlan &>(*physical_show_vindexes).collection_id() == CollectionId {8},
        "SHOW VINDEXES collection id mismatch"
    );

    DescribeCollectionPlan describe {DatabaseId {7}, CollectionId {8}, "users", loc};
    auto physical_describe = planner.plan(describe);
    require(physical_describe->kind() == PhysicalStatementPlanKind::DescribeCollection, "DESCRIBE kind mismatch");
    require(
        static_cast<const PhysicalDescribeCollectionPlan &>(*physical_describe).collection_name() == "users",
        "DESCRIBE collection name mismatch"
    );

    std::vector<BoundColumn> insert_columns {
        BoundColumn {.column_id = ColumnId {9}, .name = "age", .type = type(LogicalTypeId::Integer), .nullable = true},
    };
    std::vector<std::unique_ptr<BoundExpression>> values;
    values.push_back(literal(LogicalTypeId::Integer, "18"));
    InsertPlan insert {DatabaseId {7}, CollectionId {8}, "users", std::move(insert_columns), std::move(values), loc};
    auto physical_insert = planner.plan(insert);
    require(physical_insert->kind() == PhysicalStatementPlanKind::Insert, "INSERT kind mismatch");
    const auto & lowered_insert = static_cast<const PhysicalInsertPlan &>(*physical_insert);
    require(lowered_insert.collection_id() == CollectionId {8}, "INSERT collection id mismatch");
    require(lowered_insert.values().size() == 1, "INSERT values mismatch");
}

void test_lower_row_mutation_statement()
{
    DeletePlan logical {
        std::make_unique<LogicalScan>(DatabaseId {1}, CollectionId {2}, "users", loc),
        DatabaseId {1},
        CollectionId {2},
        "users",
        loc,
    };

    PhysicalPlanner planner;
    auto physical = planner.plan(logical);
    require(physical->kind() == PhysicalStatementPlanKind::Delete, "DELETE statement kind mismatch");

    const auto & del = static_cast<const PhysicalDeletePlan &>(*physical);
    require(del.database_id() == DatabaseId {1}, "DELETE database id mismatch");
    require(del.collection_id() == CollectionId {2}, "DELETE collection id mismatch");
    require(del.collection_name() == "users", "DELETE collection name mismatch");
    require(del.input().kind() == PhysicalPlanNodeKind::SeqScan, "DELETE input should lower to seq scan");

    std::vector<BoundAssignment> assignments;
    assignments.push_back(BoundAssignment {
        .column = BoundColumn {
            .column_id = ColumnId {3},
            .name = "age",
            .type = type(LogicalTypeId::Integer),
            .nullable = true,
        },
        .value = literal(LogicalTypeId::Integer, "42"),
    });
    UpdatePlan update {
        std::make_unique<LogicalScan>(DatabaseId {1}, CollectionId {2}, "users", loc),
        DatabaseId {1},
        CollectionId {2},
        "users",
        std::move(assignments),
        loc,
    };

    auto physical_update = planner.plan(update);
    require(physical_update->kind() == PhysicalStatementPlanKind::Update, "UPDATE statement kind mismatch");

    const auto & lowered_update = static_cast<const PhysicalUpdatePlan &>(*physical_update);
    require(lowered_update.collection_id() == CollectionId {2}, "UPDATE collection id mismatch");
    require(lowered_update.assignments().size() == 1, "UPDATE assignment count mismatch");
    require(lowered_update.assignments()[0].column.column_id == ColumnId {3}, "UPDATE assignment column mismatch");
    require(lowered_update.input().kind() == PhysicalPlanNodeKind::SeqScan, "UPDATE input should lower to seq scan");
}

} // namespace

int main()
{
    try {
        test_lower_unary_chain();
        test_lower_index_scan();
        test_lower_query_statement();
        test_lower_simple_statement();
        test_lower_row_mutation_statement();
    } catch (const std::exception & e) {
        std::cerr << "physical_planner_tests failed: " << e.what() << '\n';
        return 1;
    }

    std::cout << "physical_planner_tests passed\n";
    return 0;
}
