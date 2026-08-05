#include "core/physical_planner/operator/debug/debug_printer.hpp"

#include <ostream>
#include <sstream>
#include <string>

#include "core/binder/bound/debug/debug_printer.hpp"
#include "core/binder/bound/debug/debug_helper.hpp"
#include "core/common/value.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"

namespace litedb::core::physical_planner::op
{

PhysicalOperatorDebugPrinter::PhysicalOperatorDebugPrinter(std::ostream & ostream)
    : ostream_(ostream)
{
}

void PhysicalOperatorDebugPrinter::print(const PhysicalOperator & op)
{
    dispatch_operator(op);
}

void PhysicalOperatorDebugPrinter::visit_seq_scan_operator(const SeqScanOperator & op)
{
    header("SeqScan");
    ++indent_;
    field("collection_id", op.collection_id());
    --indent_;
}

void PhysicalOperatorDebugPrinter::visit_index_scan_operator(const IndexScanOperator & op)
{
    header("IndexScan");
    ++indent_;
    field("collection_id", op.collection_id());
    field("index_id", op.index_id());
    field("lookup", op.lookup().kind == IndexLookupKind::Equal ? "equal" : "range");
    if (op.lookup().lower.has_value()) {
        const auto value = common::value_to_string(op.lookup().lower->key.value())
            + (op.lookup().lower->inclusive ? " (inclusive)" : " (exclusive)");
        field("lower", value);
    }
    if (op.lookup().upper.has_value()) {
        const auto value = common::value_to_string(op.lookup().upper->key.value())
            + (op.lookup().upper->inclusive ? " (inclusive)" : " (exclusive)");
        field("upper", value);
    }
    --indent_;
}

void PhysicalOperatorDebugPrinter::visit_vector_search_operator(const VectorSearchOperator & op)
{
    header("VectorSearch");
    ++indent_;
    field("collection_id", op.collection_id());
    field("index_id", op.index_id());
    field("column_id", op.column_id());
    field("metric", binder::bound::vector_distance_metric_name(op.metric()));
    field("required_count", op.required_count());
    expression("query_vector", op.query_vector());
    if (op.predicate() != nullptr) {
        expression("predicate", *op.predicate());
    }
    --indent_;
}

void PhysicalOperatorDebugPrinter::visit_filter_operator(const FilterOperator & op)
{
    header("Filter");
    ++indent_;
    expression("predicate", op.predicate());
    child(op.child());
    --indent_;
}

void PhysicalOperatorDebugPrinter::visit_projection_operator(const ProjectionOperator & op)
{
    header("Projection");
    ++indent_;
    field("projection_count", op.projections().size());
    for (const auto & item : op.projections()) {
        expression("expression", *item.expression);
    }
    child(op.child());
    --indent_;
}

void PhysicalOperatorDebugPrinter::visit_sort_operator(const SortOperator & op)
{
    header("Sort");
    ++indent_;
    field("order_by_count", op.order_by().size());
    for (const auto & item : op.order_by()) {
        field("ascending", item.ascending ? 1 : 0);
        expression("expression", *item.expression);
    }
    child(op.child());
    --indent_;
}

void PhysicalOperatorDebugPrinter::visit_limit_operator(const LimitOperator & op)
{
    header("Limit");
    ++indent_;
    if (op.limit().has_value()) field("limit", *op.limit());
    if (op.offset().has_value()) field("offset", *op.offset());
    child(op.child());
    --indent_;
}

void PhysicalOperatorDebugPrinter::indent()
{
    for (std::size_t i = 0; i < indent_; ++i) ostream_ << "  ";
}

void PhysicalOperatorDebugPrinter::header(std::string_view name)
{
    indent();
    ostream_ << name << '\n';
}

void PhysicalOperatorDebugPrinter::field(std::string_view name, std::string_view value)
{
    indent();
    ostream_ << name << ": " << value << '\n';
}

void PhysicalOperatorDebugPrinter::field(std::string_view name, std::size_t value)
{
    indent();
    ostream_ << name << ": " << value << '\n';
}

void PhysicalOperatorDebugPrinter::expression(
    std::string_view name,
    const binder::bound::BoundExpression & value
)
{
    std::ostringstream stream;
    binder::bound::debug_print(stream, value);
    indent();
    ostream_ << name << ": " << stream.str() << '\n';
}

void PhysicalOperatorDebugPrinter::child(const PhysicalOperator & value)
{
    indent();
    ostream_ << "child:\n";
    ++indent_;
    print(value);
    --indent_;
}

std::string debug_print(const PhysicalOperator & op)
{
    std::ostringstream stream;
    debug_print(stream, op);
    return stream.str();
}

void debug_print(std::ostream & ostream, const PhysicalOperator & op)
{
    PhysicalOperatorDebugPrinter printer {ostream};
    printer.print(op);
}

} // namespace litedb::core::physical_planner::op
