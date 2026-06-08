#include "core/binder/binder.hpp"
#include "core/catalog/in_memory_catalog.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/planner/logical/logical_plan_node.hpp"
#include "core/planner/logical/logical_planner.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace
{

using namespace litedb::core::binder;
using namespace litedb::core::binder::bound;
using namespace litedb::core::catalog;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::planner::logical;

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

std::unique_ptr<LogicalPlanNode> plan_ok(Fixture & fixture, std::string_view sql)
{
    LogicalPlanner planner;
    auto result = planner.plan(bind_ok(fixture, sql));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

void test_select_full_chain()
{
    Fixture fixture;
    auto plan = plan_ok(
        fixture,
        "SELECT * FROM users WHERE age >= 18 ORDER BY id DESC LIMIT 10 OFFSET 5;"
    );

    require(plan->kind() == LogicalPlanNodeKind::Limit, "SELECT root should be limit");
    const auto & limit = static_cast<const LogicalLimit &>(*plan);
    require(limit.limit().value() == 10, "SELECT limit mismatch");
    require(limit.offset().value() == 5, "SELECT offset mismatch");

    require(limit.child().kind() == LogicalPlanNodeKind::OrderBy, "SELECT child should be order by");
    const auto & order_by = static_cast<const LogicalOrderBy &>(limit.child());
    require(order_by.order_by().size() == 1, "SELECT order count mismatch");
    require(!order_by.order_by()[0].ascending, "SELECT order direction mismatch");

    require(order_by.child().kind() == LogicalPlanNodeKind::Projection, "SELECT order child should be projection");
    const auto & projection = static_cast<const LogicalProjection &>(order_by.child());
    require(projection.projections().size() == 4, "SELECT wildcard projection count mismatch");

    require(projection.child().kind() == LogicalPlanNodeKind::Filter, "SELECT projection child should be filter");
    const auto & filter = static_cast<const LogicalFilter &>(projection.child());
    require(filter.predicate().type().id == LogicalTypeId::Boolean, "SELECT filter predicate type mismatch");

    require(filter.child().kind() == LogicalPlanNodeKind::Scan, "SELECT filter child should be scan");
    const auto & scan = static_cast<const LogicalScan &>(filter.child());
    require(scan.database_id() == fixture.database_id, "SELECT scan database id mismatch");
    require(scan.collection_id() == fixture.users_id, "SELECT scan collection id mismatch");
    require(scan.collection_name() == "users", "SELECT scan collection name mismatch");
}

void test_select_minimal_chain()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "SELECT id, name FROM users;");

    require(plan->kind() == LogicalPlanNodeKind::Projection, "minimal SELECT root should be projection");
    const auto & projection = static_cast<const LogicalProjection &>(*plan);
    require(projection.projections().size() == 2, "minimal SELECT projection count mismatch");
    require(projection.child().kind() == LogicalPlanNodeKind::Scan, "minimal SELECT child should be scan");
}

void test_insert_plan()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "INSERT INTO users (id, age, embedding) VALUES (1, 18, [0.1, 0.2, 0.3]);");

    require(plan->kind() == LogicalPlanNodeKind::Insert, "INSERT kind mismatch");
    const auto & insert = static_cast<const LogicalInsert &>(*plan);
    require(insert.database_id() == fixture.database_id, "INSERT database id mismatch");
    require(insert.collection_id() == fixture.users_id, "INSERT collection id mismatch");
    require(insert.columns().size() == 4, "INSERT columns count mismatch");
    require(insert.values().size() == 4, "INSERT values count mismatch");
}

void test_update_delete_plans()
{
    Fixture fixture;
    auto update = plan_ok(fixture, "UPDATE users SET age = age + 1 WHERE id = 1;");
    require(update->kind() == LogicalPlanNodeKind::Update, "UPDATE kind mismatch");
    const auto & update_node = static_cast<const LogicalUpdate &>(*update);
    require(update_node.assignments().size() == 1, "UPDATE assignment count mismatch");
    require(update_node.child().kind() == LogicalPlanNodeKind::Filter, "UPDATE with WHERE should have filter child");
    const auto & update_filter = static_cast<const LogicalFilter &>(update_node.child());
    require(update_filter.child().kind() == LogicalPlanNodeKind::Scan, "UPDATE filter child should be scan");

    auto update_without_where = plan_ok(fixture, "UPDATE users SET age = 20;");
    require(update_without_where->kind() == LogicalPlanNodeKind::Update, "UPDATE without WHERE kind mismatch");
    const auto & update_without_where_node = static_cast<const LogicalUpdate &>(*update_without_where);
    require(update_without_where_node.child().kind() == LogicalPlanNodeKind::Scan, "UPDATE without WHERE should scan directly");

    auto del = plan_ok(fixture, "DELETE FROM users WHERE id = 1;");
    require(del->kind() == LogicalPlanNodeKind::Delete, "DELETE kind mismatch");
    const auto & delete_node = static_cast<const LogicalDelete &>(*del);
    require(delete_node.child().kind() == LogicalPlanNodeKind::Filter, "DELETE with WHERE should have filter child");

    auto delete_without_where = plan_ok(fixture, "DELETE FROM users;");
    require(delete_without_where->kind() == LogicalPlanNodeKind::Delete, "DELETE without WHERE kind mismatch");
    const auto & delete_without_where_node = static_cast<const LogicalDelete &>(*delete_without_where);
    require(delete_without_where_node.child().kind() == LogicalPlanNodeKind::Scan, "DELETE without WHERE should scan directly");
}

void test_admin_and_ddl_plans()
{
    Fixture fixture;

    auto use = plan_ok(fixture, "USE demo;");
    require(use->kind() == LogicalPlanNodeKind::Use, "USE kind mismatch");
    require(static_cast<const LogicalUse &>(*use).database_id() == fixture.database_id, "USE database id mismatch");

    auto create_database = plan_ok(fixture, "CREATE DATABASE demo2;");
    require(create_database->kind() == LogicalPlanNodeKind::CreateDatabase, "CREATE DATABASE kind mismatch");
    require(static_cast<const LogicalCreateDatabase &>(*create_database).database_name() == "demo2", "CREATE DATABASE name mismatch");

    auto create_collection = plan_ok(fixture, "CREATE COLLECTION posts (id BIGINT PRIMARY KEY, embedding VECTOR(3));");
    require(create_collection->kind() == LogicalPlanNodeKind::CreateCollection, "CREATE COLLECTION kind mismatch");
    const auto & create_collection_node = static_cast<const LogicalCreateCollection &>(*create_collection);
    require(create_collection_node.database_id() == fixture.database_id, "CREATE COLLECTION database id mismatch");
    require(create_collection_node.columns().size() == 2, "CREATE COLLECTION column count mismatch");

    auto drop_database = plan_ok(fixture, "DROP DATABASE IF EXISTS missing;");
    require(drop_database->kind() == LogicalPlanNodeKind::DropDatabase, "DROP DATABASE kind mismatch");
    const auto & drop_database_node = static_cast<const LogicalDropDatabase &>(*drop_database);
    require(drop_database_node.if_exists(), "DROP DATABASE if exists mismatch");
    require(drop_database_node.database_name() == "missing", "DROP DATABASE name mismatch");

    auto drop_collection = plan_ok(fixture, "DROP COLLECTION IF EXISTS missing;");
    require(drop_collection->kind() == LogicalPlanNodeKind::DropCollection, "DROP COLLECTION kind mismatch");
    const auto & drop_collection_node = static_cast<const LogicalDropCollection &>(*drop_collection);
    require(drop_collection_node.database_id() == fixture.database_id, "DROP COLLECTION database id mismatch");
    require(drop_collection_node.if_exists(), "DROP COLLECTION if exists mismatch");

    require(plan_ok(fixture, "SHOW DATABASES;")->kind() == LogicalPlanNodeKind::ShowDatabases, "SHOW DATABASES kind mismatch");

    auto show_collections = plan_ok(fixture, "SHOW COLLECTIONS;");
    require(show_collections->kind() == LogicalPlanNodeKind::ShowCollections, "SHOW COLLECTIONS kind mismatch");
    require(static_cast<const LogicalShowCollections &>(*show_collections).database_id() == fixture.database_id, "SHOW COLLECTIONS database id mismatch");

    auto describe = plan_ok(fixture, "DESCRIBE users;");
    require(describe->kind() == LogicalPlanNodeKind::DescribeCollection, "DESCRIBE kind mismatch");
    const auto & describe_node = static_cast<const LogicalDescribeCollection &>(*describe);
    require(describe_node.database_id() == fixture.database_id, "DESCRIBE database id mismatch");
    require(describe_node.collection_id() == fixture.users_id, "DESCRIBE collection id mismatch");
}

void test_null_statement_error()
{
    LogicalPlanner planner;
    auto result = planner.plan(nullptr);
    require(!result.has_value(), "null statement should fail");
    require(result.error().code == PlannerErrorCode::InvalidArgument, "null statement error code mismatch");
}

} // namespace

int main()
{
    try {
        test_select_full_chain();
        test_select_minimal_chain();
        test_insert_plan();
        test_update_delete_plans();
        test_admin_and_ddl_plans();
        test_null_statement_error();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
