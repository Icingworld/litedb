#include "physical_planner_test_support.hpp"

#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"
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

void test_vector_top_k_selection_and_fallback()
{
    auto fixture = make_planner_catalog();
    const auto id = fixture.editor.view().find_column(fixture.collection_id, "id");
    const auto embedding = fixture.editor.view().find_column(fixture.vector_id);
    require(id.has_value() && embedding.has_value(), "vector fixture columns missing");
    physical_planner::PhysicalPlanner planner {fixture.editor.view()};

    auto make_query = [&](std::string_view function_name,
                          bool ascending,
                          std::optional<std::size_t> limit,
                          std::optional<std::size_t> offset,
                          bool with_filter = true) {
        std::unique_ptr<LogicalPlanOperator> input = std::make_unique<LogicalScanOperator>(fixture.collection_id);
        if (with_filter) {
            input = std::make_unique<LogicalFilterOperator>(std::move(input), boolean_literal(true));
        }
        std::vector<BoundProjectionItem> projections;
        projections.push_back(BoundProjectionItem {.expression = column_ref(*id), .output_name = "id"});
        std::vector<BoundOrderByItem> order_by;
        order_by.push_back(BoundOrderByItem {
            .expression = vector_distance(*embedding, function_name, {1.0, 0.0, 0.0}),
            .ascending = ascending,
        });
        return std::make_unique<QueryPlan>(std::make_unique<LogicalLimitOperator>(
            std::make_unique<LogicalOrderByOperator>(
                std::make_unique<LogicalProjectionOperator>(std::move(input), std::move(projections)),
                std::move(order_by)
            ),
            limit,
            offset
        ));
    };

    const auto inspect_vector = [&](std::string_view function_name,
                                    bool ascending,
                                    VIndexId expected_index,
                                    std::size_t limit,
                                    std::size_t offset,
                                    bool with_filter) {
        auto physical = planner.plan(make_query(function_name, ascending, limit, offset, with_filter));
        const auto & query = static_cast<const physical_planner::plan::QueryPlan &>(*physical);
        const auto & root = static_cast<const physical_planner::op::LimitOperator &>(query.root_operator());
        require(root.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
                "vector TopK must retain exact result ordering");
        const auto & sort = static_cast<const physical_planner::op::SortOperator &>(root.child());
        require(sort.order_by().size() == 1 && sort.order_by().front().ascending == ascending,
                "vector TopK sort contract mismatch");
        require(sort.order_by().front().expression->kind() == BoundExpressionKind::Function,
                "vector TopK retained sort expression was replaced");
        const auto & projection = static_cast<const physical_planner::op::ProjectionOperator &>(sort.child());
        const auto & search = static_cast<const physical_planner::op::VectorSearchOperator &>(projection.child());
        require(search.index_id() == expected_index, "vector index selection mismatch");
        require(search.required_count() == limit + offset, "vector required count mismatch");
        require(search.query_vector().kind() == BoundExpressionKind::Literal,
                "vector query constant should be materialized independently from the retained sort expression");
        require(search.predicate().has_value() == with_filter, "vector filter ownership mismatch");
        require(search.metric() == (function_name == "l2_distance"
                                        ? catalog::entry::VectorDistanceMetric::L2
                                        : function_name == "cosine_distance"
                                            ? catalog::entry::VectorDistanceMetric::Cosine
                                            : catalog::entry::VectorDistanceMetric::InnerProduct),
                "vector metric mismatch");
    };

    inspect_vector("l2_distance", true, fixture.l2_index_id, 2, 3, true);
    inspect_vector("cosine_distance", true, fixture.cosine_index_id, 3, 1, false);
    inspect_vector("inner_product", false, fixture.inner_product_index_id, 4, 0, true);

    auto wrong_direction = planner.plan(make_query("l2_distance", false, 2, 0));
    const auto & wrong_direction_query = static_cast<const physical_planner::plan::QueryPlan &>(*wrong_direction);
    const auto & wrong_direction_root = static_cast<const physical_planner::op::LimitOperator &>(wrong_direction_query.root_operator());
    require(wrong_direction_root.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
            "incompatible vector sort direction should fall back to Sort");

    auto overflowing = planner.plan(make_query(
        "l2_distance",
        true,
        std::numeric_limits<std::size_t>::max(),
        1,
        true
    ));
    const auto & overflowing_query = static_cast<const physical_planner::plan::QueryPlan &>(*overflowing);
    const auto & overflowing_root = static_cast<const physical_planner::op::LimitOperator &>(overflowing_query.root_operator());
    require(overflowing_root.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
            "overflowing vector limit should fall back to Sort");

    CatalogEditor empty_catalog;
    physical_planner::PhysicalPlanner no_index_planner {empty_catalog.view()};
    auto no_index = no_index_planner.plan(make_query("l2_distance", true, 2, 0));
    const auto & no_index_query = static_cast<const physical_planner::plan::QueryPlan &>(*no_index);
    const auto & no_index_root = static_cast<const physical_planner::op::LimitOperator &>(no_index_query.root_operator());
    require(no_index_root.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
            "missing vector index should fall back to Sort");
}

} // namespace

int main()
{
    try {
        test_vector_top_k_selection_and_fallback();
    } catch (const std::exception & error) {
        std::cerr << "physical_vector_top_k_tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "physical_vector_top_k_tests passed\n";
    return 0;
}
