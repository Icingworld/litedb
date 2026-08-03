#include "core/logical_planner/plan/debug/debug_printer.hpp"

#include <ostream>
#include <sstream>
#include <string>

#include "core/binder/bound/debug/debug_helper.hpp"
#include "core/binder/bound/debug/debug_printer.hpp"
#include "core/logical_planner/operator/debug/debug_printer.hpp"

namespace litedb::core::logical_planner::plan
{

class LogicalPlanDebugPrinter::IndentScope
{
public:
    explicit IndentScope(LogicalPlanDebugPrinter & printer) noexcept
        : printer_(printer)
    {
        ++printer_.indent_;
    }

    ~IndentScope() noexcept
    {
        --printer_.indent_;
    }

private:
    LogicalPlanDebugPrinter & printer_;     ///< 打印器
};

LogicalPlanDebugPrinter::LogicalPlanDebugPrinter(std::ostream & ostream)
    : ostream_(ostream)
    , indent_(0)
    , pending_str_()
{
}

void LogicalPlanDebugPrinter::print(const LogicalPlan & plan)
{
    dispatch_plan(plan);
}

void LogicalPlanDebugPrinter::visit_use_plan(const UsePlan & plan)
{
    write_node_header("UsePlan");
    IndentScope scope(*this);
    write_field("database_id", plan.database_id());
}

void LogicalPlanDebugPrinter::visit_create_database_plan(
    const CreateDatabasePlan & plan
)
{
    write_node_header("CreateDatabasePlan");
    IndentScope scope(*this);
    write_optional_field("database_name", plan.database_name());
}

void LogicalPlanDebugPrinter::visit_create_collection_plan(
    const CreateCollectionPlan & plan
)
{
    write_node_header("CreateCollectionPlan");
    IndentScope scope(*this);
    write_field("database_id", plan.database_id());
    write_optional_field("collection_name", plan.collection_name());
    write_optional_field("comment", plan.comment());

    write_indent();
    ostream_ << "columns:";
    if (plan.columns().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope columns_scope(*this);
    for (std::size_t index = 0; index < plan.columns().size(); ++index) {
        const auto & column = plan.columns()[index];
        write_indent();
        ostream_ << '[' << index << "] ColumnDefinition\n";
        IndentScope column_scope(*this);
        write_field("name", column.name);
        write_field(
            "type",
            binder::bound::logical_type_text(column.type)
        );
        write_field("unique", column.unique);
        write_field("nullable", column.nullable);
        write_field(
            "default_expression",
            column.default_expression.has_value()
                ? std::string_view("<present>")
                : std::string_view("<none>")
        );
        write_optional_field("comment", column.comment);
    }
}

void LogicalPlanDebugPrinter::visit_create_index_plan(
    const CreateIndexPlan & plan
)
{
    write_node_header("CreateIndexPlan");
    IndentScope scope(*this);
    write_field("column_id", plan.column_id());
    write_optional_field("index_name", plan.index_name());
    write_field(
        "index_kind",
        binder::bound::index_kind_name(plan.index_kind())
    );
    write_field("unique", plan.unique());
}

void LogicalPlanDebugPrinter::visit_create_vector_index_plan(
    const CreateVectorIndexPlan & plan
)
{
    write_node_header("CreateVectorIndexPlan");
    IndentScope scope(*this);
    write_field("column_id", plan.column_id());
    write_optional_field("vector_index_name", plan.vector_index_name());
    write_field(
        "vector_index_kind",
        binder::bound::vector_index_kind_name(plan.vector_index_kind())
    );
    write_field(
        "metric",
        binder::bound::vector_distance_metric_name(plan.metric())
    );
    write_field("max_neighbors", plan.max_neighbors());
    write_field("ef_construction", plan.ef_construction());
    write_field("ef_search_default", plan.ef_search_default());
    write_field("random_seed", plan.random_seed());
}

void LogicalPlanDebugPrinter::visit_drop_database_plan(
    const DropDatabasePlan & plan
)
{
    write_node_header("DropDatabasePlan");
    IndentScope scope(*this);
    write_optional_field("database_id", plan.database_id());
}

void LogicalPlanDebugPrinter::visit_drop_collection_plan(
    const DropCollectionPlan & plan
)
{
    write_node_header("DropCollectionPlan");
    IndentScope scope(*this);
    write_optional_field("collection_id", plan.collection_id());
}

void LogicalPlanDebugPrinter::visit_drop_index_plan(
    const DropIndexPlan & plan
)
{
    write_node_header("DropIndexPlan");
    IndentScope scope(*this);
    write_optional_field("index_id", plan.index_id());
}

void LogicalPlanDebugPrinter::visit_drop_vector_index_plan(
    const DropVectorIndexPlan & plan
)
{
    write_node_header("DropVectorIndexPlan");
    IndentScope scope(*this);
    write_optional_field("vector_index_id", plan.vector_index_id());
}

void LogicalPlanDebugPrinter::visit_show_databases_plan(
    const ShowDatabasesPlan & /*plan*/
)
{
    write_node_header("ShowDatabasesPlan");
}

void LogicalPlanDebugPrinter::visit_show_collections_plan(
    const ShowCollectionsPlan & plan
)
{
    write_node_header("ShowCollectionsPlan");
    IndentScope scope(*this);
    write_field("database_id", plan.database_id());
}

void LogicalPlanDebugPrinter::visit_show_indexes_plan(
    const ShowIndexesPlan & plan
)
{
    write_node_header("ShowIndexesPlan");
    IndentScope scope(*this);
    write_field("collection_id", plan.collection_id());
}

void LogicalPlanDebugPrinter::visit_show_vector_indexes_plan(
    const ShowVectorIndexesPlan & plan
)
{
    write_node_header("ShowVectorIndexesPlan");
    IndentScope scope(*this);
    write_field("collection_id", plan.collection_id());
}

void LogicalPlanDebugPrinter::visit_describe_collection_plan(
    const DescribeCollectionPlan & plan
)
{
    write_node_header("DescribeCollectionPlan");
    IndentScope scope(*this);
    write_field("collection_id", plan.collection_id());
}

void LogicalPlanDebugPrinter::visit_insert_plan(const InsertPlan & plan)
{
    write_node_header("InsertPlan");
    IndentScope scope(*this);
    write_field("collection_id", plan.collection_id());

    write_indent();
    ostream_ << "values:";
    if (plan.values().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope values_scope(*this);
    for (std::size_t index = 0; index < plan.values().size(); ++index) {
        write_expression_field(
            '[' + std::to_string(index) + ']',
            *plan.values()[index]
        );
    }
}

void LogicalPlanDebugPrinter::visit_update_plan(const UpdatePlan & plan)
{
    write_node_header("UpdatePlan");
    IndentScope scope(*this);
    write_field("collection_id", plan.collection_id());

    write_indent();
    ostream_ << "assignments:";
    if (plan.assignments().empty()) {
        ostream_ << " []\n";
    } else {
        ostream_ << '\n';
        IndentScope assignments_scope(*this);
        for (std::size_t index = 0; index < plan.assignments().size(); ++index) {
            const auto & assignment = plan.assignments()[index];
            write_indent();
            ostream_ << '[' << index << "] BoundAssignment\n";
            IndentScope assignment_scope(*this);
            write_field("column_id", assignment.column_id);
            write_expression_field("value", *assignment.value);
        }
    }

    write_operator_field("root_operator", &plan.root_operator());
}

void LogicalPlanDebugPrinter::visit_delete_plan(const DeletePlan & plan)
{
    write_node_header("DeletePlan");
    IndentScope scope(*this);
    write_field("collection_id", plan.collection_id());
    write_operator_field("root_operator", &plan.root_operator());
}

void LogicalPlanDebugPrinter::visit_query_plan(const QueryPlan & plan)
{
    write_node_header("QueryPlan");
    IndentScope scope(*this);
    write_operator_field("root_operator", &plan.root_operator());
}

void LogicalPlanDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        ostream_ << "  ";
    }
}

void LogicalPlanDebugPrinter::write_node_header(std::string_view name)
{
    write_indent();
    if (!pending_str_.empty()) {
        ostream_ << pending_str_;
        pending_str_.clear();
    }
    ostream_ << name << '\n';
}

void LogicalPlanDebugPrinter::write_field(
    std::string_view name,
    std::string_view value
)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void LogicalPlanDebugPrinter::write_field(
    std::string_view name,
    bool value
)
{
    write_field(
        name,
        value ? std::string_view("true") : std::string_view("false")
    );
}

void LogicalPlanDebugPrinter::write_field(
    std::string_view name,
    std::size_t value
)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void LogicalPlanDebugPrinter::write_optional_field(
    std::string_view name,
    const std::optional<std::string> & value
)
{
    write_field(
        name,
        value ? std::string_view(*value) : std::string_view("<none>")
    );
}

void LogicalPlanDebugPrinter::write_optional_field(
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

void LogicalPlanDebugPrinter::write_expression_field(
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

void LogicalPlanDebugPrinter::write_operator_field(
    std::string_view name,
    const op::LogicalPlanOperator * child
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
    const auto text = op::debug_print(*child);
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }

        write_indent();
        ostream_ << text.substr(start, end - start) << '\n';
        start = end + 1;
    }
}

std::string debug_print(const LogicalPlan & plan)
{
    std::ostringstream stream;
    debug_print(stream, plan);
    return stream.str();
}

void debug_print(
    std::ostream & ostream,
    const LogicalPlan & plan
)
{
    LogicalPlanDebugPrinter printer(ostream);
    printer.print(plan);
}

} // namespace litedb::core::logical_planner::plan
