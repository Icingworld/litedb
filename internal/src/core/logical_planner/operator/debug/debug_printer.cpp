#include "core/logical_planner/operator/debug/debug_printer.hpp"

#include <ostream>
#include <sstream>
#include <string>

#include "core/binder/bound/debug/debug_helper.hpp"
#include "core/binder/bound/debug/debug_printer.hpp"

namespace litedb::core::logical_planner::op
{

class LogicalPlanOperatorDebugPrinter::IndentScope
{
public:
    explicit IndentScope(LogicalPlanOperatorDebugPrinter & printer) noexcept
        : printer_(printer)
    {
        ++printer_.indent_;
    }

    ~IndentScope() noexcept
    {
        --printer_.indent_;
    }

private:
    LogicalPlanOperatorDebugPrinter & printer_;
};

LogicalPlanOperatorDebugPrinter::LogicalPlanOperatorDebugPrinter(std::ostream & ostream)
    : ostream_(ostream)
    , indent_(0)
    , pending_str_()
{}

void LogicalPlanOperatorDebugPrinter::print(const LogicalPlanOperator & op)
{
    dispatch_operator(op);
}

void LogicalPlanOperatorDebugPrinter::visit_scan_operator(const LogicalScanOperator & op)
{
    write_node_header("LogicalScanOperator");
    IndentScope scope(*this);
    write_field("collection_id", op.collection_id());
}

void LogicalPlanOperatorDebugPrinter::visit_filter_operator(const LogicalFilterOperator & op)
{
    write_node_header("LogicalFilterOperator");
    IndentScope scope(*this);
    write_expression_field("predicate", op.predicate());
    write_child_field("child", &op.child());
}

void LogicalPlanOperatorDebugPrinter::visit_projection_operator(
    const LogicalProjectionOperator & op
)
{
    write_node_header("LogicalProjectionOperator");
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

void LogicalPlanOperatorDebugPrinter::visit_order_by_operator(const LogicalOrderByOperator & op)
{
    write_node_header("LogicalOrderByOperator");
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

void LogicalPlanOperatorDebugPrinter::visit_limit_operator(const LogicalLimitOperator & op)
{
    write_node_header("LogicalLimitOperator");
    IndentScope scope(*this);
    write_optional_field("limit", op.limit());
    write_optional_field("offset", op.offset());
    write_child_field("child", &op.child());
}

void LogicalPlanOperatorDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        ostream_ << "  ";
    }
}

void LogicalPlanOperatorDebugPrinter::write_node_header(std::string_view name)
{
    write_indent();
    if (!pending_str_.empty()) {
        ostream_ << pending_str_;
        pending_str_.clear();
    }
    ostream_ << name << '\n';
}

void LogicalPlanOperatorDebugPrinter::write_field(std::string_view name, std::string_view value)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void LogicalPlanOperatorDebugPrinter::write_field(std::string_view name, bool value)
{
    write_field(name, value ? std::string_view("true") : std::string_view("false"));
}

void LogicalPlanOperatorDebugPrinter::write_field(std::string_view name, std::size_t value)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void LogicalPlanOperatorDebugPrinter::write_optional_field(
    std::string_view name,
    const std::optional<std::string> & value
)
{
    write_field(name, value ? std::string_view(*value) : std::string_view("<none>"));
}

void LogicalPlanOperatorDebugPrinter::write_optional_field(
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

void LogicalPlanOperatorDebugPrinter::write_type_field(
    std::string_view name,
    const common::LogicalType & type
)
{
    write_field(name, binder::bound::logical_type_text(type));
}

void LogicalPlanOperatorDebugPrinter::write_expression_field(
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

void LogicalPlanOperatorDebugPrinter::write_child_field(
    std::string_view name,
    const LogicalPlanOperator * child
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

std::string debug_print(const LogicalPlanOperator & op)
{
    std::ostringstream stream;
    debug_print(stream, op);
    return stream.str();
}

void debug_print(std::ostream & ostream, const LogicalPlanOperator & op)
{
    LogicalPlanOperatorDebugPrinter printer(ostream);
    printer.print(op);
}

} // namespace litedb::core::logical_planner::op
