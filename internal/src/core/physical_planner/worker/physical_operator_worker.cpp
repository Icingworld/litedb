#include "core/physical_planner/worker/physical_operator_worker.hpp"

#include <cassert>
#include <utility>

#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"

namespace litedb::core::physical_planner
{

std::unique_ptr<op::PhysicalOperator> PhysicalOperatorWorker::lower_operator(
    std::unique_ptr<logical_planner::op::LogicalPlanOperator> logical_operator
)
{
    assert(logical_operator != nullptr);
    return dispatch_operator(*logical_operator.release());
}

std::unique_ptr<op::PhysicalOperator> PhysicalOperatorWorker::visit_scan_operator(
    logical_planner::op::LogicalScanOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalScanOperator> owned(&logical_operator);
    return std::make_unique<op::SeqScanOperator>(owned->collection_id());
}

std::unique_ptr<op::PhysicalOperator> PhysicalOperatorWorker::visit_filter_operator(
    logical_planner::op::LogicalFilterOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalFilterOperator> owned(&logical_operator);
    const auto & logical_child = owned->child();

    if (logical_child.kind() == logical_planner::op::LogicalPlanOperatorKind::Scan) {
        const auto & scan = static_cast<const logical_planner::op::LogicalScanOperator &>(
            logical_child
        );
        const auto path = scalar_selector_.select(scan.collection_id(), owned->predicate());
        if (path.has_value()) {
            auto predicate = owned->take_predicate();
            auto physical_scan = std::make_unique<op::IndexScanOperator>(
                scan.collection_id(),
                path->index_id,
                std::move(path->lookup)
            );
            return std::make_unique<op::FilterOperator>(
                std::move(physical_scan),
                std::move(predicate)
            );
        }
    }

    auto child = lower_operator(owned->take_child());
    auto predicate = owned->take_predicate();
    return std::make_unique<op::FilterOperator>(
        std::move(child),
        std::move(predicate)
    );
}

std::unique_ptr<op::PhysicalOperator> PhysicalOperatorWorker::visit_projection_operator(
    logical_planner::op::LogicalProjectionOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalProjectionOperator> owned(&logical_operator);
    auto child = lower_operator(owned->take_child());
    return std::make_unique<op::ProjectionOperator>(
        std::move(child),
        owned->take_projections()
    );
}

std::unique_ptr<op::PhysicalOperator> PhysicalOperatorWorker::visit_order_by_operator(
    logical_planner::op::LogicalOrderByOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalOrderByOperator> owned(&logical_operator);
    auto child = lower_operator(owned->take_child());
    return std::make_unique<op::SortOperator>(
        std::move(child),
        owned->take_order_by()
    );
}

std::unique_ptr<op::PhysicalOperator> PhysicalOperatorWorker::visit_limit_operator(
    logical_planner::op::LogicalLimitOperator & logical_operator
)
{
    std::unique_ptr<logical_planner::op::LogicalLimitOperator> owned(&logical_operator);

    auto decision = vector_selector_.select(*owned);
    if (decision.has_value()) {
        auto order = std::unique_ptr<logical_planner::op::LogicalOrderByOperator>(
            static_cast<logical_planner::op::LogicalOrderByOperator *>(
                owned->take_child().release()
            )
        );
        auto projection = std::unique_ptr<logical_planner::op::LogicalProjectionOperator>(
            static_cast<logical_planner::op::LogicalProjectionOperator *>(
                order->take_child().release()
            )
        );

        auto base = projection->take_child();
        std::unique_ptr<binder::bound::BoundExpression> predicate;
        if (decision->has_filter) {
            auto filter = std::unique_ptr<logical_planner::op::LogicalFilterOperator>(
                static_cast<logical_planner::op::LogicalFilterOperator *>(
                    base.release()
                )
            );
            predicate = filter->take_predicate();
            base = filter->take_child();
        }

        auto scan = std::unique_ptr<logical_planner::op::LogicalScanOperator>(
            static_cast<logical_planner::op::LogicalScanOperator *>(base.release())
        );

        auto order_items = order->take_order_by();
        auto projection_items = projection->take_projections();
        auto query_vector = std::make_unique<binder::bound::BoundLiteralExpression>(
            decision->query_type,
            std::move(decision->query_value)
        );
        auto search = std::make_unique<op::VectorSearchOperator>(
            decision->collection_id,
            decision->index_id,
            decision->column_id,
            decision->metric,
            std::move(query_vector),
            std::move(predicate),
            decision->required_count
        );
        auto projected = std::make_unique<op::ProjectionOperator>(
            std::move(search),
            std::move(projection_items)
        );
        auto sorted = std::make_unique<op::SortOperator>(
            std::move(projected),
            std::move(order_items)
        );
        return std::make_unique<op::LimitOperator>(
            std::move(sorted),
            owned->limit(),
            owned->offset()
        );
    }

    auto child = lower_operator(owned->take_child());
    return std::make_unique<op::LimitOperator>(
        std::move(child),
        owned->limit(),
        owned->offset()
    );
}

} // namespace litedb::core::physical_planner
