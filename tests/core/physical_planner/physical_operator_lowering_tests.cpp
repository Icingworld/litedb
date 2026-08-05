#include "physical_planner_test_support.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <vector>

#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;
using namespace litedb::core::logical_planner::op;
using namespace litedb::core::logical_planner::plan;
using namespace physical_planner_test_support;

void test_recursive_lowering_and_shape()
{
    meta::CatalogEditor catalog;
    physical_planner::PhysicalPlanner planner {catalog.view()};
    std::vector<BoundProjectionItem> projections;
    projections.push_back(BoundProjectionItem {.expression = integer_literal(1), .output_name = "constant"});
    std::vector<BoundOrderByItem> order_by;
    order_by.push_back(BoundOrderByItem {.expression = integer_literal(1), .ascending = false});
    auto logical = std::make_unique<QueryPlan>(
        std::make_unique<LogicalLimitOperator>(
            std::make_unique<LogicalOrderByOperator>(
                std::make_unique<LogicalProjectionOperator>(
                    std::make_unique<LogicalFilterOperator>(
                        std::make_unique<LogicalScanOperator>(42),
                        boolean_literal(true)
                    ),
                    std::move(projections)
                ),
                std::move(order_by)
            ),
            10,
            2
        )
    );

    auto physical = planner.plan(std::move(logical));
    const auto & query = static_cast<const physical_planner::plan::QueryPlan &>(*physical);
    const auto & limit = static_cast<const physical_planner::op::LimitOperator &>(query.root_operator());
    const auto & sort = static_cast<const physical_planner::op::SortOperator &>(limit.child());
    const auto & projection = static_cast<const physical_planner::op::ProjectionOperator &>(sort.child());
    const auto & filter = static_cast<const physical_planner::op::FilterOperator &>(projection.child());
    const auto & scan = static_cast<const physical_planner::op::SeqScanOperator &>(filter.child());
    require(scan.collection_id() == 42, "scan collection id lowering mismatch");
    require(limit.limit() == 10 && limit.offset() == 2, "limit values lowering mismatch");
    require(projection.projections().size() == 1 && sort.order_by().size() == 1,
            "projection/order ownership lowering mismatch");
}

} // namespace

int main()
{
    try {
        test_recursive_lowering_and_shape();
    } catch (const std::exception & error) {
        std::cerr << "physical_operator_lowering_tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "physical_operator_lowering_tests passed\n";
    return 0;
}
