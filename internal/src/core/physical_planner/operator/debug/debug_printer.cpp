#include "core/physical_planner/operator/debug/debug_printer.hpp"

#include <ostream>
#include <sstream>
#include <string>

#include "core/binder/bound/debug/debug_helper.hpp"
#include "core/binder/bound/debug/debug_printer.hpp"
#include "core/common/value.hpp"

namespace litedb::core::physical_planner::op
{

class PhysicalOperatorDebugPrinter::IndentScope
{
public:
    explicit IndentScope(
        PhysicalOperatorDebugPrinter & printer
    ) noexcept
        : printer_(printer)
    {
        ++printer_.indent_;
    }

    ~IndentScope() noexcept
    {
        --printer_.indent_;
    }

private:
    PhysicalOperatorDebugPrinter & printer_;     // 打印器
};

PhysicalOperatorDebugPrinter::PhysicalOperatorDebugPrinter(
    std::ostream & ostream
)
    : ostream_(ostream)
    , indent_(0)
    , pending_str_()
{
}

void PhysicalOperatorDebugPrinter::print(const PhysicalOperator & op)
{
    dispatch_operator(op);
}

void PhysicalOperatorDebugPrinter::visit_seq_scan_operator(
    const SeqScanOperator & op
)
{
    write_node_header("SeqScanOperator");
    IndentScope scope(*this);
    write_field("collection_id", op.collection_id());
}

void PhysicalOperatorDebugPrinter::visit_index_scan_operator(
    const IndexScanOperator & op
)
{
    write_node_header("IndexScanOperator");
    IndentScope scope(*this);
    write_field("collection_id", op.collection_id());
    write_field("index_id", op.index_id());
    write_field(
        "lookup",
        op.lookup().kind == IndexLookupKind::Equal
            ? std::string_view("equal")
            : std::string_view("range")
    );

    if (op.lookup().lower.has_value()) {
        const auto value = common::value_to_string(op.lookup().lower->key.value())
            + (op.lookup().lower->inclusive ? " (inclusive)" : " (exclusive)");
        write_field("lower", value);
    } else {
        write_field("lower", "<none>");
    }

    if (op.lookup().upper.has_value()) {
        const auto value = common::value_to_string(op.lookup().upper->key.value())
            + (op.lookup().upper->inclusive ? " (inclusive)" : " (exclusive)");
        write_field("upper", value);
    } else {
        write_field("upper", "<none>");
    }
}

void PhysicalOperatorDebugPrinter::visit_vector_search_operator(
    const VectorSearchOperator & op
)
{
    write_node_header("VectorSearchOperator");
    IndentScope scope(*this);
    write_field("collection_id", op.collection_id());
    write_field("index_id", op.index_id());
    write_field("column_id", op.column_id());
    write_field("metric", binder::bound::vector_distance_metric_name(op.metric()));
    write_field("required_count", op.required_count());
    write_expression_field("query_vector", op.query_vector());
    if (op.predicate().has_value()) {
        write_expression_field("predicate", op.predicate()->get());
    } else {
        write_field("predicate", "<none>");
    }
}

void PhysicalOperatorDebugPrinter::visit_filter_operator(
    const FilterOperator & op
)
{
    write_node_header("FilterOperator");
    IndentScope scope(*this);
    write_expression_field("predicate", op.predicate());
    write_child_field("child", &op.child());
}

void PhysicalOperatorDebugPrinter::visit_projection_operator(
    const ProjectionOperator & op
)
{
    write_node_header("ProjectionOperator");
    IndentScope scope(*this);
    write_field("projection_count", op.projections().size());

    for (std::size_t index = 0; index < op.projections().size(); ++index) {
        const auto & projection = op.projections()[index];
        write_indent();
        ostream_ << '[' << index << "] ProjectionItem\n";
        IndentScope item_scope(*this);
        write_field("output_name", projection.output_name);
        write_expression_field("expression", *projection.expression);
    }

    write_child_field("child", &op.child());
}

void PhysicalOperatorDebugPrinter::visit_sort_operator(
    const SortOperator & op
)
{
    write_node_header("SortOperator");
    IndentScope scope(*this);
    write_field("order_by_count", op.order_by().size());

    for (std::size_t index = 0; index < op.order_by().size(); ++index) {
        const auto & order_by = op.order_by()[index];
        write_indent();
        ostream_ << '[' << index << "] OrderByItem\n";
        IndentScope item_scope(*this);
        write_field("ascending", order_by.ascending);
        write_expression_field("expression", *order_by.expression);
    }

    write_child_field("child", &op.child());
}

void PhysicalOperatorDebugPrinter::visit_limit_operator(
    const LimitOperator & op
)
{
    write_node_header("LimitOperator");
    IndentScope scope(*this);
    write_optional_field("limit", op.limit());
    write_optional_field("offset", op.offset());
    write_child_field("child", &op.child());
}

void PhysicalOperatorDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        ostream_ << "  ";
    }
}

void PhysicalOperatorDebugPrinter::write_node_header(std::string_view name)
{
    write_indent();
    if (!pending_str_.empty()) {
        ostream_ << pending_str_;
        pending_str_.clear();
    }
    ostream_ << name << '\n';
}

void PhysicalOperatorDebugPrinter::write_field(
    std::string_view name,
    std::string_view value
)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void PhysicalOperatorDebugPrinter::write_field(
    std::string_view name,
    bool value
)
{
    write_field(
        name,
        value ? std::string_view("true") : std::string_view("false")
    );
}

void PhysicalOperatorDebugPrinter::write_field(
    std::string_view name,
    std::size_t value
)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void PhysicalOperatorDebugPrinter::write_optional_field(
    std::string_view name,
    const std::optional<std::string> & value
)
{
    write_field(
        name,
        value ? std::string_view(*value) : std::string_view("<none>")
    );
}

void PhysicalOperatorDebugPrinter::write_optional_field(
    std::string_view name,
    const std::optional<std::size_t> & value
)
{
    write_indent();
    ostream_ << name << ": ";
    if (value) {
        ostream_ << *value;
    } else {
        ostream_ << "<none>";
    }
    ostream_ << '\n';
}

void PhysicalOperatorDebugPrinter::write_expression_field(
    std::string_view name,
    const binder::bound::BoundExpression & expression
)
{
    write_indent();
    ostream_ << name << ":\n";

    const auto text = binder::bound::debug_print(expression);
    const auto child_indent = indent_ + 1;
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }

        for (std::size_t index = 0; index < child_indent; ++index) {
            ostream_ << "  ";
        }
        ostream_ << text.substr(start, end - start) << '\n';
        start = end + 1;
    }
}

void PhysicalOperatorDebugPrinter::write_child_field(
    std::string_view name,
    const PhysicalOperator * child
)
{
    write_indent();
    ostream_ << name << ':';
    if (child == nullptr) {
        ostream_ << " <none>\n";
        return;
    }

    ostream_ << '\n';
    IndentScope scope(*this);
    print(*child);
}

std::string debug_print(const PhysicalOperator & op)
{
    std::ostringstream stream;
    debug_print(stream, op);
    return stream.str();
}

void debug_print(
    std::ostream & ostream,
    const PhysicalOperator & op
)
{
    PhysicalOperatorDebugPrinter printer(ostream);
    printer.print(op);
}

} // namespace litedb::core::physical_planner::op
