#include "core/binder/binder.hpp"
#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/common/types.hpp"
#include "core/function/builtin/builtin_functions.hpp"
#include "core/function/scalar_function.hpp"
#include "core/logical_planner/logical_planner.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/optimizer/optimizer.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using namespace litedb::core::binder;
using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::logical_planner;
using namespace litedb::core::logical_planner::op;
using namespace litedb::core::logical_planner::plan;
using namespace litedb::core::meta;
using namespace litedb::core::optimizer;
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

struct Fixture
{
    CatalogEditor catalog;
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        require(database.has_value(), "fixture database create failed");
        database_id = *database;

        CreateCollectionRequest users;
        users.database_id = database_id;
        users.name = "users";
        users.columns = {
            ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt), .nullable = false},
            ColumnDefinition {.name = "name", .type = type(LogicalTypeId::Varchar, 64), .nullable = true},
            ColumnDefinition {.name = "age", .type = type(LogicalTypeId::Integer), .nullable = true},
            ColumnDefinition {.name = "embedding", .type = type(LogicalTypeId::Vector, 3), .nullable = true},
        };

        auto collection = catalog.create_collection(users);
        require(collection.has_value(), "fixture collection create failed");
        users_id = *collection;
    }
};

std::unique_ptr<litedb::core::parser::ast::StatementNode>
parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

std::unique_ptr<BoundStatement> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    BinderContext context {
        fixture.catalog.view(),
        session,
        litedb::core::function::builtin::builtin_function_catalog(),
    };
    Binder binder {context};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

std::unique_ptr<LogicalPlan> plan_ok(Fixture & fixture, std::string_view sql)
{
    LogicalPlanner planner;
    return planner.plan(bind_ok(fixture, sql));
}

std::unique_ptr<LogicalPlan> optimize_ok(
    std::unique_ptr<LogicalPlan> plan,
    OptimizerOptions options = {}
)
{
    Optimizer optimizer {options};
    return optimizer.optimize(std::move(plan));
}

const LogicalPlanOperator & query_root(const LogicalPlan & plan)
{
    require(plan.kind() == LogicalPlanKind::Query, "plan should be query");
    return static_cast<const QueryPlan &>(plan).root_operator();
}

const LogicalProjectionOperator & query_projection(const LogicalPlan & plan)
{
    const auto & root = query_root(plan);
    require(root.kind() == LogicalPlanOperatorKind::Projection, "query root should be projection");
    return static_cast<const LogicalProjectionOperator &>(root);
}

const LogicalFilterOperator & query_filter(const LogicalPlan & plan)
{
    const auto & projection = query_projection(plan);
    require(projection.child().kind() == LogicalPlanOperatorKind::Filter, "projection child should be filter");
    return static_cast<const LogicalFilterOperator &>(projection.child());
}

bool numeric_value_is(const Value & value, std::int64_t expected)
{
    return std::visit(
        [expected](const auto & data) {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
                return data == expected;
            } else {
                return false;
            }
        },
        value.data()
    );
}

void require_literal_value(const BoundExpression & expression, std::int64_t expected)
{
    require(expression.kind() == BoundExpressionKind::Literal, "expression should be literal");
    const auto & literal = static_cast<const BoundLiteralExpression &>(expression);
    require(numeric_value_is(literal.value(), expected), "literal value mismatch");
}

std::unique_ptr<BoundExpression> integer_literal(std::int32_t value)
{
    return std::make_unique<BoundLiteralExpression>(
        type(LogicalTypeId::Integer),
        Value {ValueData {value}}
    );
}

std::unique_ptr<BoundExpression> boolean_literal(bool value)
{
    return std::make_unique<BoundLiteralExpression>(
        type(LogicalTypeId::Boolean),
        Value {ValueData {value}}
    );
}

void require_boolean_literal_value(const BoundExpression & expression, bool expected)
{
    require(
        expression.kind() == BoundExpressionKind::Literal,
        "expression should be boolean literal"
    );
    const auto & literal = static_cast<const BoundLiteralExpression &>(expression);
    const auto * value = std::get_if<bool>(&literal.value().data());
    require(value != nullptr && *value == expected, "boolean literal value mismatch");
}

std::unique_ptr<BoundExpression> failing_boolean_expression()
{
    auto division = std::make_unique<BoundBinaryExpression>(
        integer_literal(1),
        BinaryOperator::Divide,
        integer_literal(0),
        type(LogicalTypeId::Integer)
    );
    return std::make_unique<BoundBinaryExpression>(
        std::move(division),
        BinaryOperator::Equal,
        integer_literal(0),
        type(LogicalTypeId::Boolean)
    );
}

std::unique_ptr<LogicalPlan> make_projection_plan(
    CollectionId collection_id,
    std::unique_ptr<BoundExpression> expression
)
{
    std::vector<BoundProjectionItem> projections;
    projections.push_back(BoundProjectionItem {
        .expression = std::move(expression),
        .output_name = "expr",
    });
    return std::make_unique<QueryPlan>(
        std::make_unique<LogicalProjectionOperator>(
            std::make_unique<LogicalScanOperator>(collection_id),
            std::move(projections)
        )
    );
}

std::unique_ptr<LogicalPlan> optimize_boolean_expression(
    Fixture & fixture,
    std::unique_ptr<BoundExpression> left,
    BinaryOperator op,
    std::unique_ptr<BoundExpression> right
)
{
    return optimize_ok(make_projection_plan(
        fixture.users_id,
        std::make_unique<BoundBinaryExpression>(
            std::move(left),
            op,
            std::move(right),
            type(LogicalTypeId::Boolean)
        )
    ));
}

void test_disabled_optimizer_preserves_identity()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "SELECT id FROM users WHERE true;");
    auto * original = plan.get();

    OptimizerOptions options;
    options.enabled = false;
    auto optimized = optimize_ok(std::move(plan), options);

    require(optimized.get() == original, "disabled optimizer should preserve plan pointer");
    const auto & projection = query_projection(*optimized);
    require(projection.child().kind() == LogicalPlanOperatorKind::Filter, "disabled optimizer should preserve Filter");
}

void test_query_insert_update_delete_expression_rewrite()
{
    Fixture fixture;

    auto query = optimize_ok(plan_ok(
        fixture,
        "SELECT 10 + 8 FROM users WHERE age > 10 + 8;"
    ));
    require_literal_value(*query_projection(*query).projections()[0].expression, 18);
    const auto & predicate = query_filter(*query).predicate();
    require(predicate.kind() == BoundExpressionKind::Binary, "query predicate should remain comparison");
    const auto & comparison = static_cast<const BoundBinaryExpression &>(predicate);
    require_literal_value(comparison.right(), 18);

    auto insert = optimize_ok(plan_ok(
        fixture,
        "INSERT INTO users (id, age) VALUES (10 + 8, 20 + 1);"
    ));
    const auto & insert_plan = static_cast<const InsertPlan &>(*insert);
    require_literal_value(*insert_plan.values()[0], 18);
    require(insert_plan.values()[1]->kind() == BoundExpressionKind::Null, "unspecified INSERT column should remain NULL");
    require_literal_value(*insert_plan.values()[2], 21);

    auto update = optimize_ok(plan_ok(
        fixture,
        "UPDATE users SET age = 10 + 8 WHERE true;"
    ));
    const auto & update_plan = static_cast<const UpdatePlan &>(*update);
    require(update_plan.root_operator().kind() == LogicalPlanOperatorKind::Scan, "UPDATE Filter(true) should be removed");
    require_literal_value(*update_plan.assignments()[0].value, 18);

    auto delete_plan = optimize_ok(plan_ok(fixture, "DELETE FROM users WHERE true;"));
    require(
        static_cast<const DeletePlan &>(*delete_plan).root_operator().kind()
            == LogicalPlanOperatorKind::Scan,
        "DELETE Filter(true) should be removed"
    );
}

void test_constant_folding_scope_and_errors()
{
    Fixture fixture;

    auto nested = optimize_ok(plan_ok(fixture, "SELECT -(10 + 8) FROM users;"));
    require_literal_value(*query_projection(*nested).projections()[0].expression, -18);

    auto null_plan = optimize_ok(make_projection_plan(
        fixture.users_id,
        std::make_unique<BoundBinaryExpression>(
            std::make_unique<BoundNullExpression>(type(LogicalTypeId::Integer)),
            BinaryOperator::Add,
            integer_literal(1),
            type(LogicalTypeId::Integer)
        )
    ));
    require(
        query_projection(*null_plan).projections()[0].expression->kind() == BoundExpressionKind::Null,
        "NULL arithmetic should fold to typed NULL"
    );

    auto error_plan = optimize_ok(make_projection_plan(
        fixture.users_id,
        std::make_unique<BoundBinaryExpression>(
            integer_literal(1),
            BinaryOperator::Divide,
            integer_literal(0),
            type(LogicalTypeId::Integer)
        )
    ));
    require(
        query_projection(*error_plan).projections()[0].expression->kind()
            == BoundExpressionKind::Binary,
        "evaluator errors should preserve the expression"
    );

    auto cast_plan = optimize_ok(make_projection_plan(
        fixture.users_id,
        std::make_unique<BoundCastExpression>(
            integer_literal(18),
            type(LogicalTypeId::BigInt)
        )
    ));
    require_literal_value(*query_projection(*cast_plan).projections()[0].expression, 18);
}

void test_boolean_simplification_is_short_circuit_safe()
{
    Fixture fixture;

    const auto check_expression = [&](std::string_view sql, auto check) {
        auto plan = optimize_ok(plan_ok(fixture, sql));
        check(*query_projection(*plan).projections()[0].expression);
    };

    check_expression(
        "SELECT true AND age > 18 FROM users;",
        [](const BoundExpression & expression) {
            require(
                static_cast<const BoundBinaryExpression &>(expression).op() ==
                    BinaryOperator::GreaterThan,
                "true AND x should return x"
            );
        }
    );
    check_expression(
        "SELECT false AND age > 18 FROM users;",
        [](const BoundExpression & expression) {
            require_boolean_literal_value(expression, false);
        }
    );
    check_expression(
        "SELECT age > 18 AND true FROM users;",
        [](const BoundExpression & expression) {
            require(
                static_cast<const BoundBinaryExpression &>(expression).op() ==
                    BinaryOperator::GreaterThan,
                "x AND true should return x"
            );
        }
    );
    check_expression(
        "SELECT age > 18 AND false FROM users;",
        [](const BoundExpression & expression) {
            require(
                static_cast<const BoundBinaryExpression &>(expression).op() == BinaryOperator::And,
                "x AND false must be preserved"
            );
        }
    );
    check_expression("SELECT true OR age > 18 FROM users;", [](const BoundExpression & expression) {
        require_boolean_literal_value(expression, true);
    });
    check_expression(
        "SELECT false OR age > 18 FROM users;",
        [](const BoundExpression & expression) {
            require(
                static_cast<const BoundBinaryExpression &>(expression).op() ==
                    BinaryOperator::GreaterThan,
                "false OR x should return x"
            );
        }
    );
    check_expression(
        "SELECT age > 18 OR false FROM users;",
        [](const BoundExpression & expression) {
            require(
                static_cast<const BoundBinaryExpression &>(expression).op() ==
                    BinaryOperator::GreaterThan,
                "x OR false should return x"
            );
        }
    );
    check_expression("SELECT age > 18 OR true FROM users;", [](const BoundExpression & expression) {
        require(
            static_cast<const BoundBinaryExpression &>(expression).op() == BinaryOperator::Or,
            "x OR true must be preserved"
        );
    });
}

void test_boolean_short_circuit_preserves_error_order()
{
    Fixture fixture;

    auto false_and_error = optimize_boolean_expression(
        fixture,
        boolean_literal(false),
        BinaryOperator::And,
        failing_boolean_expression()
    );
    require_boolean_literal_value(
        *query_projection(*false_and_error).projections()[0].expression,
        false
    );

    auto true_or_error = optimize_boolean_expression(
        fixture,
        boolean_literal(true),
        BinaryOperator::Or,
        failing_boolean_expression()
    );
    require_boolean_literal_value(
        *query_projection(*true_or_error).projections()[0].expression,
        true
    );

    auto error_and_false = optimize_boolean_expression(
        fixture,
        failing_boolean_expression(),
        BinaryOperator::And,
        boolean_literal(false)
    );
    require(
        query_projection(*error_and_false).projections()[0].expression->kind() ==
            BoundExpressionKind::Binary,
        "error AND false should remain binary"
    );
    const auto & preserved_and = static_cast<const BoundBinaryExpression &>(
        *query_projection(*error_and_false).projections()[0].expression
    );
    require(
        preserved_and.op() == BinaryOperator::And,
        "error AND false must preserve left-to-right evaluation"
    );

    auto error_or_true = optimize_boolean_expression(
        fixture,
        failing_boolean_expression(),
        BinaryOperator::Or,
        boolean_literal(true)
    );
    require(
        query_projection(*error_or_true).projections()[0].expression->kind() ==
            BoundExpressionKind::Binary,
        "error OR true should remain binary"
    );
    const auto & preserved_or = static_cast<const BoundBinaryExpression &>(
        *query_projection(*error_or_true).projections()[0].expression
    );
    require(
        preserved_or.op() == BinaryOperator::Or,
        "error OR true must preserve left-to-right evaluation"
    );
}

void test_boolean_null_semantics()
{
    Fixture fixture;

    const auto null_boolean = [] {
        return std::make_unique<BoundNullExpression>(type(LogicalTypeId::Boolean));
    };
    auto null_and_false = optimize_boolean_expression(
        fixture,
        null_boolean(),
        BinaryOperator::And,
        boolean_literal(false)
    );
    require_boolean_literal_value(
        *query_projection(*null_and_false).projections()[0].expression,
        false
    );

    auto null_and_true = optimize_boolean_expression(
        fixture,
        null_boolean(),
        BinaryOperator::And,
        boolean_literal(true)
    );
    require(
        query_projection(*null_and_true).projections()[0].expression->kind() ==
            BoundExpressionKind::Null,
        "NULL AND true should remain NULL"
    );

    auto null_or_true = optimize_boolean_expression(
        fixture,
        null_boolean(),
        BinaryOperator::Or,
        boolean_literal(true)
    );
    require_boolean_literal_value(
        *query_projection(*null_or_true).projections()[0].expression,
        true
    );

    auto null_or_false = optimize_boolean_expression(
        fixture,
        null_boolean(),
        BinaryOperator::Or,
        boolean_literal(false)
    );
    require(
        query_projection(*null_or_false).projections()[0].expression->kind() ==
            BoundExpressionKind::Null,
        "NULL OR false should remain NULL"
    );
}

void test_optimizer_is_idempotent()
{
    Fixture fixture;
    auto optimized = optimize_ok(plan_ok(fixture, "SELECT 10 + 8 FROM users WHERE true;"));

    const auto require_optimized_shape = [](const LogicalPlan & plan) {
        const auto & projection = query_projection(plan);
        require(
            projection.child().kind() == LogicalPlanOperatorKind::Scan,
            "Filter(true) should remain eliminated"
        );
        require_literal_value(*projection.projections()[0].expression, 18);
    };

    require_optimized_shape(*optimized);
    optimized = optimize_ok(std::move(optimized));
    require_optimized_shape(*optimized);
}

void test_expression_rewriter_releases_consumed_nodes()
{
    Fixture fixture;
    std::weak_ptr<const litedb::core::function::ScalarFunctionOverload> overload_lifetime;

    {
        auto overload = std::make_shared<litedb::core::function::ScalarFunctionOverload>();
        overload->return_type = type(LogicalTypeId::Integer);
        overload_lifetime = overload;

        litedb::core::function::BoundScalarFunction function {
            "optimizer_lifetime_probe",
            overload,
            {},
            type(LogicalTypeId::Integer),
            nullptr,
            0,
        };
        auto plan = make_projection_plan(
            fixture.users_id,
            std::make_unique<BoundFunctionExpression>(
                std::move(function),
                std::vector<std::unique_ptr<BoundExpression>> {}
            )
        );
        auto optimized = optimize_ok(std::move(plan));
        overload.reset();
        require(
            !overload_lifetime.expired(),
            "optimized function should retain its overload while alive"
        );
    }

    require(
        overload_lifetime.expired(),
        "consumed function expression should release its overload"
    );
}

void test_filter_elimination_and_operator_order()
{
    Fixture fixture;

    auto true_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE true;"));
    require(
        query_projection(*true_plan).child().kind() == LogicalPlanOperatorKind::Scan,
        "Filter(true) should be eliminated"
    );

    auto false_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE false;"));
    require(
        query_projection(*false_plan).child().kind() == LogicalPlanOperatorKind::Filter,
        "Filter(false) should be retained"
    );

    auto chain = optimize_ok(plan_ok(
        fixture,
        "SELECT id FROM users WHERE age > 10 ORDER BY name DESC LIMIT 2 OFFSET 1;"
    ));
    const auto & limit = static_cast<const LogicalLimitOperator &>(query_root(*chain));
    require(limit.child().kind() == LogicalPlanOperatorKind::OrderBy, "Limit/OrderBy order changed");
    const auto & order_by = static_cast<const LogicalOrderByOperator &>(limit.child());
    require(order_by.child().kind() == LogicalPlanOperatorKind::Projection, "OrderBy/Projection order changed");
    const auto & projection = static_cast<const LogicalProjectionOperator &>(order_by.child());
    require(projection.child().kind() == LogicalPlanOperatorKind::Filter, "Projection/Filter order changed");
    require(
        static_cast<const LogicalFilterOperator &>(projection.child()).child().kind()
            == LogicalPlanOperatorKind::Scan,
        "Filter/Scan order changed"
    );
}

void test_all_composite_expression_kinds_are_rewritten()
{
    Fixture fixture;
    auto plan = optimize_ok(plan_ok(
        fixture,
        "SELECT 1 IN (1 + 2, 4), 1 BETWEEN 0 + 1 AND 2 + 2, "
        "'abc' LIKE 'a%', "
        "l2_distance([0.1, 0.2, 0.3], [0.3, 0.2, 0.1]), "
        "[1 + 2, 3] FROM users;"
    ));

    const auto & projections = query_projection(*plan).projections();
    require(projections[0].expression->kind() == BoundExpressionKind::In, "IN should be retained");
    require(
        projections[1].expression->kind() == BoundExpressionKind::Between,
        "BETWEEN should be retained"
    );
    require(
        projections[2].expression->kind() == BoundExpressionKind::Like,
        "LIKE should be retained"
    );
    require(
        projections[3].expression->kind() == BoundExpressionKind::Function,
        "function should be retained"
    );
    const auto & in = static_cast<const BoundInExpression &>(*projections[0].expression);
    require_literal_value(*in.values()[0], 3);
    const auto & between = static_cast<const BoundBetweenExpression &>(*projections[1].expression);
    require_literal_value(between.lower(), 1);
    require_literal_value(between.upper(), 4);
    const auto & function =
        static_cast<const BoundFunctionExpression &>(*projections[3].expression);
    require(
        function.arguments()[0]->kind() == BoundExpressionKind::Vector,
        "function vector should be retained"
    );
    require(
        function.arguments()[1]->kind() == BoundExpressionKind::Vector,
        "function vector should be retained"
    );
    require(
        projections[4].expression->kind() == BoundExpressionKind::Vector,
        "vector should be retained"
    );
    const auto & vector = static_cast<const BoundVectorExpression &>(*projections[4].expression);
    require_literal_value(*vector.elements()[0], 3);
}

void test_command_plans_pass_through()
{
    Fixture fixture;

    const std::vector<std::pair<std::string_view, LogicalPlanKind>> commands {
        {"USE demo;", LogicalPlanKind::Use},
        {"CREATE DATABASE demo2;", LogicalPlanKind::CreateDatabase},
        {"CREATE COLLECTION posts (id BIGINT);", LogicalPlanKind::CreateCollection},
        {"CREATE INDEX IF NOT EXISTS idx_age ON users (age) USING BTREE;", LogicalPlanKind::CreateIndex},
        {"CREATE VINDEX IF NOT EXISTS vidx_embedding ON users (embedding) USING HNSW;", LogicalPlanKind::CreateVectorIndex},
        {"DROP DATABASE IF EXISTS missing;", LogicalPlanKind::DropDatabase},
        {"DROP COLLECTION IF EXISTS missing;", LogicalPlanKind::DropCollection},
        {"DROP INDEX IF EXISTS missing ON users;", LogicalPlanKind::DropIndex},
        {"DROP VINDEX IF EXISTS missing ON users;", LogicalPlanKind::DropVectorIndex},
        {"SHOW DATABASES;", LogicalPlanKind::ShowDatabases},
        {"SHOW COLLECTIONS;", LogicalPlanKind::ShowCollections},
        {"SHOW INDEXES FROM users;", LogicalPlanKind::ShowIndexes},
        {"SHOW VINDEXES FROM users;", LogicalPlanKind::ShowVectorIndexes},
        {"DESCRIBE users;", LogicalPlanKind::DescribeCollection},
    };

    for (const auto [sql, expected_kind] : commands) {
        auto plan = plan_ok(fixture, sql);
        auto * original = plan.get();
        auto optimized = optimize_ok(std::move(plan));
        require(optimized.get() == original, "command plan should pass through unchanged");
        require(optimized->kind() == expected_kind, "command plan dispatch kind mismatch");
    }
}

} // namespace

int main()
{
    try {
        test_disabled_optimizer_preserves_identity();
        test_query_insert_update_delete_expression_rewrite();
        test_constant_folding_scope_and_errors();
        test_boolean_simplification_is_short_circuit_safe();
        test_boolean_short_circuit_preserves_error_order();
        test_boolean_null_semantics();
        test_optimizer_is_idempotent();
        test_expression_rewriter_releases_consumed_nodes();
        test_filter_elimination_and_operator_order();
        test_all_composite_expression_kinds_are_rewritten();
        test_command_plans_pass_through();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
