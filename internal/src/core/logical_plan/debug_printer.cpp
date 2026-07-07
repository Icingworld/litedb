#include "core/logical_plan/debug_printer.hpp"

#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

#include "core/binder/bound/debug_printer.hpp"
#include "core/logical_plan/node/logical_filter.hpp"
#include "core/logical_plan/node/logical_index_scan.hpp"
#include "core/logical_plan/node/logical_limit.hpp"
#include "core/logical_plan/node/logical_order_by.hpp"
#include "core/logical_plan/node/logical_projection.hpp"
#include "core/logical_plan/node/logical_scan.hpp"

namespace litedb::core::planner::logical
{

namespace
{

/**
 * @brief 写入缩进行
 * @param out 输出流
 * @param indent 缩进
 * @param text 文本
 */
void write_indented_lines(std::ostream & out, std::size_t indent, std::string_view text)
{
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }

        for (std::size_t index = 0; index < indent; ++index) {
            out << "  ";
        }
        out << text.substr(start, end - start) << '\n';
        start = end + 1;
    }
}

} // namespace

class LogicalDebugPrinter::IndentScope
{
public:
    explicit IndentScope(LogicalDebugPrinter & printer) noexcept
        : printer_(printer)
    {
        ++printer_.indent_;
    }

    ~IndentScope() noexcept
    {
        --printer_.indent_;
    }

private:
    LogicalDebugPrinter & printer_;
};

LogicalDebugPrinter::LogicalDebugPrinter(std::ostream & out, LogicalDebugPrinterOptions options)
    : out_(out)
    , options_(options)
{
}

void LogicalDebugPrinter::print(const LogicalPlanNode & node)
{
    node.accept(*this);
}

void LogicalDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        out_ << "  ";
    }
}

void LogicalDebugPrinter::write_node_header(
    const char * name,
    parser::ast::AstNodeLocation location
)
{
    write_indent();
    out_ << name;
    if (options_.include_location) {
        out_ << " @ " << location.line << ':' << location.column;
    }
    out_ << '\n';
}

void LogicalDebugPrinter::write_field(const char * name, const std::string & value)
{
    write_field(name, value.c_str());
}

void LogicalDebugPrinter::write_field(const char * name, const char * value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void LogicalDebugPrinter::write_field(const char * name, bool value)
{
    write_field(name, value ? "true" : "false");
}

void LogicalDebugPrinter::write_field(const char * name, std::size_t value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void LogicalDebugPrinter::write_optional_field(const char * name, std::optional<std::size_t> value)
{
    if (!value.has_value()) {
        write_field(name, "null");
        return;
    }
    write_field(name, value.value());
}

void LogicalDebugPrinter::write_bound_expression_field(
    const char * name,
    const binder::bound::BoundExpression & expression
)
{
    write_indent();
    out_ << name << ":\n";

    binder::bound::BoundDebugPrinterOptions bound_options {
        .include_location = options_.include_location,
        .include_type = options_.include_expression_type,
    };
    const auto printed = binder::bound::debug_print(expression, bound_options);
    write_indented_lines(out_, indent_ + 1, printed);
}

void LogicalDebugPrinter::write_child_field(const char * name, const LogicalPlanNode & child)
{
    write_indent();
    out_ << name << ":\n";
    IndentScope scope {*this};
    child.accept(*this);
}

void LogicalDebugPrinter::visit(const LogicalScan & node)
{
    write_node_header("LogicalScan", node.location());
    IndentScope scope {*this};
    write_field("database_id", static_cast<std::size_t>(node.database_id()));
    write_field("collection_id", static_cast<std::size_t>(node.collection_id()));
    write_field("collection_name", node.collection_name());
}

void LogicalDebugPrinter::visit(const LogicalFilter & node)
{
    write_node_header("LogicalFilter", node.location());
    IndentScope scope {*this};
    write_bound_expression_field("predicate", node.predicate());
    write_child_field("child", node.child());
}

void LogicalDebugPrinter::visit(const LogicalIndexScan & node)
{
    write_node_header("LogicalIndexScan", node.location());
    IndentScope scope {*this};
    write_field("database_id", static_cast<std::size_t>(node.database_id()));
    write_field("collection_id", static_cast<std::size_t>(node.collection_id()));
    write_field("collection_name", node.collection_name());
    write_field("index_id", static_cast<std::size_t>(node.index_id()));
    write_field("index_name", node.index_name());
    write_field("column_id", static_cast<std::size_t>(node.column_id()));
    write_field("column_name", node.column_name());
    write_field("lookup", node.lookup().kind == LogicalIndexLookupKind::Equal ? "equal" : "range");
}

void LogicalDebugPrinter::visit(const LogicalProjection & node)
{
    write_node_header("LogicalProjection", node.location());
    IndentScope scope {*this};
    write_field("projection_count", node.projections().size());
    for (std::size_t index = 0; index < node.projections().size(); ++index) {
        write_indent();
        out_ << '[' << index << "] ProjectionItem\n";
        IndentScope item_scope {*this};
        const auto & projection = node.projections()[index];
        if (projection.alias.has_value()) {
            write_field("alias", projection.alias.value());
        }
        write_bound_expression_field("expression", *projection.expression);
    }
    write_child_field("child", node.child());
}

void LogicalDebugPrinter::visit(const LogicalOrderBy & node)
{
    write_node_header("LogicalOrderBy", node.location());
    IndentScope scope {*this};
    write_field("order_by_count", node.order_by().size());
    for (std::size_t index = 0; index < node.order_by().size(); ++index) {
        write_indent();
        out_ << '[' << index << "] OrderByItem\n";
        IndentScope item_scope {*this};
        const auto & order_by = node.order_by()[index];
        write_field("ascending", order_by.ascending);
        write_bound_expression_field("expression", *order_by.expression);
    }
    write_child_field("child", node.child());
}

void LogicalDebugPrinter::visit(const LogicalLimit & node)
{
    write_node_header("LogicalLimit", node.location());
    IndentScope scope {*this};
    write_optional_field("limit", node.limit());
    write_optional_field("offset", node.offset());
    write_child_field("child", node.child());
}

std::string debug_print(const LogicalPlanNode & node, LogicalDebugPrinterOptions options)
{
    std::ostringstream out;
    debug_print(out, node, options);
    return out.str();
}

void debug_print(std::ostream & out, const LogicalPlanNode & node, LogicalDebugPrinterOptions options)
{
    LogicalDebugPrinter printer(out, options);
    printer.print(node);
}

} // namespace litedb::core::planner::logical
