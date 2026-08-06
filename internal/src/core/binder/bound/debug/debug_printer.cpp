#include "core/binder/bound/debug/debug_printer.hpp"

#include <ostream>
#include <sstream>

#include "core/binder/bound/bound_column.hpp"
#include "core/binder/bound/debug/debug_helper.hpp"

namespace litedb::core::binder::bound
{

class BoundDebugPrinter::IndentScope
{
public:
    explicit IndentScope(BoundDebugPrinter & printer) noexcept
        : printer_(printer)
    {
        ++printer_.indent_;
    }

    ~IndentScope() noexcept
    {
        --printer_.indent_;
    }

private:
    BoundDebugPrinter & printer_;
};

BoundDebugPrinter::BoundDebugPrinter(
    std::ostream & ostream,
    BoundDebugPrinterOptions options
)
    : ostream_(ostream)
    , options_(options)
    , indent_(0)
    , pending_str_()
{
}

void BoundDebugPrinter::print(const BoundStatement & statement)
{
    dispatch_statement(statement);
}

void BoundDebugPrinter::print(const BoundExpression & expression)
{
    dispatch_expression(expression);
}

void BoundDebugPrinter::visit_create_database_statement(
    const BoundCreateDatabaseStatement & statement
)
{
    write_node_header("BoundCreateDatabaseStatement");
    IndentScope scope(*this);
    write_optional_field("database_name", statement.database_name());
}

void BoundDebugPrinter::visit_create_collection_statement(
    const BoundCreateCollectionStatement & statement
)
{
    write_node_header("BoundCreateCollectionStatement");
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_optional_field("collection_name", statement.collection_name());
    write_optional_field("comment", statement.comment());

    write_indent();
    ostream_ << "columns:";
    if (statement.columns().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope columns_scope(*this);
    for (std::size_t index = 0; index < statement.columns().size(); ++index) {
        const auto & column = statement.columns()[index];
        write_indent();
        ostream_ << '[' << index << "] ColumnDefinition\n";
        IndentScope column_scope(*this);
        write_field("name", column.name);
        write_type_field("type", column.type);
        write_field("unique", column.unique);
        write_field("nullable", column.nullable);
        write_field(
            "default_expression",
            column.default_expression.has_value() ? "<present>" : "<none>"
        );
        write_optional_field("comment", column.comment);
    }
}

void BoundDebugPrinter::visit_create_index_statement(
    const BoundCreateIndexStatement & statement
)
{
    write_node_header("BoundCreateIndexStatement");
    IndentScope scope(*this);
    write_field("column_id", statement.column_id());
    write_optional_field("index_name", statement.index_name());
    write_field("index_kind", index_kind_name(statement.index_kind()));
    write_field("unique", statement.unique());
}

void BoundDebugPrinter::visit_create_vector_index_statement(
    const BoundCreateVectorIndexStatement & statement
)
{
    write_node_header("BoundCreateVectorIndexStatement");
    IndentScope scope(*this);
    write_field("column_id", statement.column_id());
    write_optional_field("vector_index_name", statement.vector_index_name());
    write_field(
        "vector_index_kind",
        vector_index_kind_name(statement.vector_index_kind())
    );
    write_field("metric", vector_distance_metric_name(statement.metric()));
    write_field("max_neighbors", statement.max_neighbors());
    write_field("ef_construction", statement.ef_construction());
    write_field("ef_search_default", statement.ef_search_default());
    write_field("random_seed", statement.random_seed());
}

void BoundDebugPrinter::visit_delete_statement(
    const BoundDeleteStatement & statement
)
{
    write_node_header("BoundDeleteStatement");
    IndentScope scope(*this);
    write_field("collection_id", statement.collection_id());
    write_child_field("where", statement.where());
}

void BoundDebugPrinter::visit_describe_collection_statement(
    const BoundDescribeCollectionStatement & statement
)
{
    write_node_header("BoundDescribeCollectionStatement");
    IndentScope scope(*this);
    write_field("collection_id", statement.collection_id());
}

void BoundDebugPrinter::visit_drop_database_statement(
    const BoundDropDatabaseStatement & statement
)
{
    write_node_header("BoundDropDatabaseStatement");
    IndentScope scope(*this);
    write_optional_field("database_id", statement.database_id());
}

void BoundDebugPrinter::visit_drop_collection_statement(
    const BoundDropCollectionStatement & statement
)
{
    write_node_header("BoundDropCollectionStatement");
    IndentScope scope(*this);
    write_optional_field("collection_id", statement.collection_id());
}

void BoundDebugPrinter::visit_drop_index_statement(
    const BoundDropIndexStatement & statement
)
{
    write_node_header("BoundDropIndexStatement");
    IndentScope scope(*this);
    write_optional_field("index_id", statement.index_id());
}

void BoundDebugPrinter::visit_drop_vector_index_statement(
    const BoundDropVectorIndexStatement & statement
)
{
    write_node_header("BoundDropVectorIndexStatement");
    IndentScope scope(*this);
    write_optional_field("vector_index_id", statement.vector_index_id());
}

void BoundDebugPrinter::visit_insert_statement(
    const BoundInsertStatement & statement
)
{
    write_node_header("BoundInsertStatement");
    IndentScope scope(*this);
    write_field("collection_id", statement.collection_id());

    write_indent();
    ostream_ << "values:";
    if (statement.values().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope values_scope(*this);
    for (std::size_t index = 0; index < statement.values().size(); ++index) {
        pending_str_ = '[' + std::to_string(index) + "] ";
        print(*statement.values()[index]);
    }
}

void BoundDebugPrinter::visit_select_statement(
    const BoundSelectStatement & statement
)
{
    write_node_header("BoundSelectStatement");
    IndentScope scope(*this);
    write_field("collection_id", statement.collection_id());

    write_indent();
    ostream_ << "projections:";
    if (statement.projections().empty()) {
        ostream_ << " []\n";
    } else {
        ostream_ << '\n';
        IndentScope projections_scope(*this);
        for (std::size_t index = 0; index < statement.projections().size(); ++index) {
            const auto & projection = statement.projections()[index];
            write_indent();
            ostream_ << '[' << index << "] BoundProjectionItem\n";
            IndentScope item_scope(*this);
            write_field("output_name", projection.output_name);
            write_child_field("expression", projection.expression.get());
        }
    }

    write_child_field("where", statement.where());

    write_indent();
    ostream_ << "order_by:";
    if (statement.order_by().empty()) {
        ostream_ << " []\n";
    } else {
        ostream_ << '\n';
        IndentScope order_scope(*this);
        for (std::size_t index = 0; index < statement.order_by().size(); ++index) {
            const auto & order_by = statement.order_by()[index];
            write_indent();
            ostream_ << '[' << index << "] BoundOrderByItem\n";
            IndentScope item_scope(*this);
            write_child_field("expression", order_by.expression.get());
            write_field("ascending", order_by.ascending);
        }
    }

    write_optional_field("limit", statement.limit());
    write_optional_field("offset", statement.offset());
}

void BoundDebugPrinter::visit_show_databases_statement(
    const BoundShowDatabasesStatement & /*statement*/
)
{
    write_node_header("BoundShowDatabasesStatement");
}

void BoundDebugPrinter::visit_show_collections_statement(
    const BoundShowCollectionsStatement & statement
)
{
    write_node_header("BoundShowCollectionsStatement");
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
}

void BoundDebugPrinter::visit_show_indexes_statement(
    const BoundShowIndexesStatement & statement
)
{
    write_node_header("BoundShowIndexesStatement");
    IndentScope scope(*this);
    write_field("collection_id", statement.collection_id());
}

void BoundDebugPrinter::visit_show_vector_indexes_statement(
    const BoundShowVectorIndexesStatement & statement
)
{
    write_node_header("BoundShowVectorIndexesStatement");
    IndentScope scope(*this);
    write_field("collection_id", statement.collection_id());
}

void BoundDebugPrinter::visit_update_statement(
    const BoundUpdateStatement & statement
)
{
    write_node_header("BoundUpdateStatement");
    IndentScope scope(*this);
    write_field("collection_id", statement.collection_id());

    write_indent();
    ostream_ << "assignments:";
    if (statement.assignments().empty()) {
        ostream_ << " []\n";
    } else {
        ostream_ << '\n';
        IndentScope assignments_scope(*this);
        for (std::size_t index = 0; index < statement.assignments().size(); ++index) {
            const auto & assignment = statement.assignments()[index];
            write_indent();
            ostream_ << '[' << index << "] BoundAssignment\n";
            IndentScope assignment_scope(*this);
            write_field("column_id", assignment.column_id);
            write_child_field("value", assignment.value.get());
        }
    }

    write_child_field("where", statement.where());
}

void BoundDebugPrinter::visit_use_statement(
    const BoundUseStatement & statement
)
{
    write_node_header("BoundUseStatement");
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
}

void BoundDebugPrinter::visit_literal_expression(
    const BoundLiteralExpression & expression
)
{
    write_node_header("BoundLiteralExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("value", common::value_to_string(expression.value()));
}

void BoundDebugPrinter::visit_null_expression(
    const BoundNullExpression & expression
)
{
    write_node_header("BoundNullExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
}

void BoundDebugPrinter::visit_column_ref_expression(
    const BoundColumnRefExpression & expression
)
{
    write_node_header("BoundColumnRefExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("column_id", expression.column_id());
    write_field("column_ordinal", expression.column_ordinal());
}

void BoundDebugPrinter::visit_unary_expression(
    const BoundUnaryExpression & expression
)
{
    write_node_header("BoundUnaryExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("op", unary_operator_name(expression.op()));
    write_child_field("operand", &expression.operand());
}

void BoundDebugPrinter::visit_binary_expression(
    const BoundBinaryExpression & expression
)
{
    write_node_header("BoundBinaryExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("op", binary_operator_name(expression.op()));
    write_child_field("left", &expression.left());
    write_child_field("right", &expression.right());
}

void BoundDebugPrinter::visit_vector_expression(
    const BoundVectorExpression & expression
)
{
    write_node_header("BoundVectorExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_indent();
    ostream_ << "elements:";
    if (expression.elements().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope elements_scope(*this);
    for (std::size_t index = 0; index < expression.elements().size(); ++index) {
        pending_str_ = '[' + std::to_string(index) + "] ";
        print(*expression.elements()[index]);
    }
}

void BoundDebugPrinter::visit_function_expression(
    const BoundFunctionExpression & expression
)
{
    write_node_header("BoundFunctionExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("name", expression.function().name());

    write_indent();
    ostream_ << "arguments:";
    if (expression.arguments().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope arguments_scope(*this);
    for (std::size_t index = 0; index < expression.arguments().size(); ++index) {
        pending_str_ = '[' + std::to_string(index) + "] ";
        print(*expression.arguments()[index]);
    }
}

void BoundDebugPrinter::visit_in_expression(
    const BoundInExpression & expression
)
{
    write_node_header("BoundInExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_child_field("expression", &expression.expression());

    write_indent();
    ostream_ << "values:";
    if (expression.values().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope values_scope(*this);
    for (std::size_t index = 0; index < expression.values().size(); ++index) {
        pending_str_ = '[' + std::to_string(index) + "] ";
        print(*expression.values()[index]);
    }
}

void BoundDebugPrinter::visit_between_expression(
    const BoundBetweenExpression & expression
)
{
    write_node_header("BoundBetweenExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_child_field("expression", &expression.expression());
    write_child_field("lower", &expression.lower());
    write_child_field("upper", &expression.upper());
}

void BoundDebugPrinter::visit_like_expression(
    const BoundLikeExpression & expression
)
{
    write_node_header("BoundLikeExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_child_field("expression", &expression.expression());
    write_child_field("pattern", &expression.pattern());
}

void BoundDebugPrinter::visit_cast_expression(
    const BoundCastExpression & expression
)
{
    write_node_header("BoundCastExpression");
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_type_field("target_type", expression.type());
    write_child_field("expression", &expression.expression());
}

void BoundDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        ostream_ << "  ";
    }
}

void BoundDebugPrinter::write_node_header(std::string_view name)
{
    write_indent();
    if (!pending_str_.empty()) {
        ostream_ << pending_str_;
        pending_str_.clear();
    }
    ostream_ << name << '\n';
}

void BoundDebugPrinter::write_field(
    std::string_view name,
    std::string_view value
)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void BoundDebugPrinter::write_field(std::string_view name, bool value)
{
    write_field(name, value ? std::string_view("true") : std::string_view("false"));
}

void BoundDebugPrinter::write_field(std::string_view name, std::size_t value)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void BoundDebugPrinter::write_optional_field(
    std::string_view name,
    const std::optional<std::string> & value
)
{
    write_field(
        name,
        value ? std::string_view(*value) : std::string_view("<none>")
    );
}

void BoundDebugPrinter::write_optional_field(
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

void BoundDebugPrinter::write_type_field(
    std::string_view name,
    const common::LogicalType & type
)
{
    if (!options_.include_type) {
        return;
    }
    write_field(name, logical_type_text(type));
}

void BoundDebugPrinter::write_bound_column(const BoundColumn & column)
{
    write_node_header("BoundColumn");
    IndentScope scope(*this);
    write_field("column_id", column.column_id);
    write_field("name", column.name);
    write_type_field("type", column.type);
    write_field("nullable", column.nullable);
}

void BoundDebugPrinter::write_child_field(
    std::string_view name,
    const BoundExpression * expression
)
{
    write_indent();
    ostream_ << name << ':';
    if (expression == nullptr) {
        ostream_ << " <none>\n";
        return;
    }

    ostream_ << '\n';
    IndentScope scope(*this);
    print(*expression);
}

std::string debug_print(
    const BoundStatement & statement,
    BoundDebugPrinterOptions options
)
{
    std::ostringstream stream;
    debug_print(stream, statement, options);
    return stream.str();
}

std::string debug_print(
    const BoundExpression & expression,
    BoundDebugPrinterOptions options
)
{
    std::ostringstream stream;
    debug_print(stream, expression, options);
    return stream.str();
}

void debug_print(
    std::ostream & ostream,
    const BoundStatement & statement,
    BoundDebugPrinterOptions options
)
{
    BoundDebugPrinter printer(ostream, options);
    printer.print(statement);
}

void debug_print(
    std::ostream & ostream,
    const BoundExpression & expression,
    BoundDebugPrinterOptions options
)
{
    BoundDebugPrinter printer(ostream, options);
    printer.print(expression);
}

} // namespace litedb::core::binder::bound
