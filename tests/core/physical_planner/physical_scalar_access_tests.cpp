#include "physical_planner_test_support.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <variant>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::logical_planner::op;
using namespace litedb::core::logical_planner::plan;
using namespace physical_planner_test_support;

void test_scalar_selection_and_fallback()
{
    auto fixture = make_planner_catalog();
    const auto age = fixture.editor.view().find_column(fixture.age_id);
    require(age.has_value(), "scalar fixture column missing");
    physical_planner::PhysicalPlanner planner {fixture.editor.view()};

    auto plan_filter = [&](std::unique_ptr<BoundExpression> predicate) {
        return planner.plan(std::make_unique<QueryPlan>(
            std::make_unique<LogicalFilterOperator>(
                std::make_unique<LogicalScanOperator>(fixture.collection_id),
                std::move(predicate)
            )
        ));
    };

    const auto inspect_index = [&](std::unique_ptr<BoundExpression> predicate,
                                   physical_planner::op::IndexLookupKind lookup_kind) {
        auto physical = plan_filter(std::move(predicate));
        const auto & query = static_cast<const physical_planner::plan::QueryPlan &>(*physical);
        const auto & filter = static_cast<const physical_planner::op::FilterOperator &>(query.root_operator());
        const auto & scan = static_cast<const physical_planner::op::IndexScanOperator &>(filter.child());
        require(scan.index_id() == fixture.first_age_index_id, "scalar index selection was not deterministic");
        require(scan.lookup().kind == lookup_kind, "scalar lookup kind mismatch");
        require(filter.predicate().kind() != BoundExpressionKind::Null, "residual predicate was dropped");
        return scan.lookup();
    };

    const auto equal = inspect_index(
        scalar_predicate(*age, BinaryOperator::Equal, small_integer_literal(18)),
        physical_planner::op::IndexLookupKind::Equal
    );
    require(equal.lower.has_value() && !equal.upper.has_value(), "equality lookup bounds mismatch");
    require(std::get<std::int32_t>(equal.lower->key.value().data()) == 18, "equality lookup key mismatch");

    const auto reversed = inspect_index(
        std::make_unique<BoundBinaryExpression>(
            small_integer_literal(18),
            BinaryOperator::LessThan,
            column_ref(*age),
            LogicalType {LogicalTypeId::Boolean, std::nullopt}
        ),
        physical_planner::op::IndexLookupKind::Range
    );
    require(reversed.lower.has_value() && !reversed.lower->inclusive,
            "reversed comparison was not normalized to a lower bound");

    const auto upper = inspect_index(
        scalar_predicate(*age, BinaryOperator::LessThanOrEqual, small_integer_literal(40)),
        physical_planner::op::IndexLookupKind::Range
    );
    require(upper.upper.has_value() && upper.upper->inclusive, "upper range bound mismatch");

    const auto between = inspect_index(
        std::make_unique<BoundBetweenExpression>(
            column_ref(*age),
            small_integer_literal(10),
            small_integer_literal(20)
        ),
        physical_planner::op::IndexLookupKind::Range
    );
    require(between.lower.has_value() && between.upper.has_value(), "BETWEEN bounds missing");
    require(std::get<std::int32_t>(between.lower->key.value().data()) == 10
                && std::get<std::int32_t>(between.upper->key.value().data()) == 20,
            "BETWEEN lookup bounds mismatch");

    auto non_constant = plan_filter(scalar_predicate(*age, BinaryOperator::Equal, column_ref(*age)));
    const auto & non_constant_query = static_cast<const physical_planner::plan::QueryPlan &>(*non_constant);
    const auto & non_constant_filter = static_cast<const physical_planner::op::FilterOperator &>(non_constant_query.root_operator());
    require(non_constant_filter.child().kind() == physical_planner::op::PhysicalOperatorKind::SeqScan,
            "non-constant scalar predicate should fall back to SeqScan");

    auto null_constant = plan_filter(scalar_predicate(
        *age,
        BinaryOperator::Equal,
        literal(LogicalTypeId::Null, Value::null())
    ));
    const auto & null_query = static_cast<const physical_planner::plan::QueryPlan &>(*null_constant);
    const auto & null_filter = static_cast<const physical_planner::op::FilterOperator &>(null_query.root_operator());
    require(null_filter.child().kind() == physical_planner::op::PhysicalOperatorKind::SeqScan,
            "invalid scalar key should fall back to SeqScan");
}

} // namespace

int main()
{
    try {
        test_scalar_selection_and_fallback();
    } catch (const std::exception & error) {
        std::cerr << "physical_scalar_access_tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "physical_scalar_access_tests passed\n";
    return 0;
}
