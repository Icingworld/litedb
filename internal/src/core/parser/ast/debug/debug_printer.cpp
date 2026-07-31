#include "core/parser/ast/debug/debug_printer.hpp"

#include <sstream>
#include <utility>

#include "core/parser/ast/debug/debug_helper.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 缩进作用域
 */
class AstDebugPrinter::IndentScope
{
public:
    explicit IndentScope(AstDebugPrinter & printer) noexcept
        : printer_(printer)
    {
        ++printer_.indent_;
    }

    ~IndentScope() noexcept
    {
        --printer_.indent_;
    }

private:
    AstDebugPrinter & printer_;
};

AstDebugPrinter::AstDebugPrinter(
    std::ostream & ostream,
    AstDebugPrinterOptions options
)
    : ostream_(ostream)
    , options_(options)
    , indent_(0)
    , pending_str_()
{
}

void AstDebugPrinter::print(const AstNode & node)
{
    switch (node.kind()) {
    case AstNodeKind::CreateDatabase:
        [[fallthrough]];
    case AstNodeKind::CreateCollection:
        [[fallthrough]];
    case AstNodeKind::CreateIndex:
        [[fallthrough]];
    case AstNodeKind::CreateVectorIndex:
        [[fallthrough]];
    case AstNodeKind::Delete:
        [[fallthrough]];
    case AstNodeKind::DescribeCollection:
        [[fallthrough]];
    case AstNodeKind::DropDatabase:
        [[fallthrough]];
    case AstNodeKind::DropCollection:
        [[fallthrough]];
    case AstNodeKind::DropIndex:
        [[fallthrough]];
    case AstNodeKind::DropVectorIndex:
        [[fallthrough]];
    case AstNodeKind::Insert:
        [[fallthrough]];
    case AstNodeKind::Select:
        [[fallthrough]];
    case AstNodeKind::ShowDatabases:
        [[fallthrough]];
    case AstNodeKind::ShowCollections:
        [[fallthrough]];
    case AstNodeKind::ShowIndexes:
        [[fallthrough]];
    case AstNodeKind::ShowVectorIndexes:
        [[fallthrough]];
    case AstNodeKind::Update:
        [[fallthrough]];
    case AstNodeKind::Use:
        print(static_cast<const StatementNode &>(node));
        return;

    case AstNodeKind::Identifier:
        [[fallthrough]];
    case AstNodeKind::Wildcard:
        [[fallthrough]];
    case AstNodeKind::Literal:
        [[fallthrough]];
    case AstNodeKind::FunctionCall:
        [[fallthrough]];
    case AstNodeKind::ColumnReference:
        [[fallthrough]];
    case AstNodeKind::Vector:
        [[fallthrough]];
    case AstNodeKind::Binary:
        [[fallthrough]];
    case AstNodeKind::Unary:
        [[fallthrough]];
    case AstNodeKind::In:
        [[fallthrough]];
    case AstNodeKind::Between:
        [[fallthrough]];
    case AstNodeKind::Like:
        [[fallthrough]];
    case AstNodeKind::Alias:
        print(static_cast<const ExpressionNode &>(node));
        return;
    }

    std::unreachable();
}

void AstDebugPrinter::print(const StatementNode & statement)
{
    dispatch_statement(statement);
}

void AstDebugPrinter::print(const ExpressionNode & expression)
{
    dispatch_expression(expression);
}

void AstDebugPrinter::visit_create_database_statement(
    const CreateDatabaseStatement & statement
)
{
    write_node_header("CreateDatabaseStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_name", statement.database_name());
    write_field("if_not_exists", statement.if_not_exists());
}

void AstDebugPrinter::visit_create_collection_statement(
    const CreateCollectionStatement & statement
)
{
    write_node_header("CreateCollectionStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection", statement.collection_name());
    write_field("if_not_exists", statement.if_not_exists());
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
        ostream_ << '[' << index << "] ColumnDefinitionSyntax\n";
        IndentScope column_scope(*this);
        write_field("name", column.name);
        write_indent();
        ostream_ << "type:\n";
        {
            IndentScope type_scope(*this);
            write_field("kind", logical_type_name(column.type.id));
            write_optional_field("parameter", column.type.parameter);
        }
        write_field("unique", column.unique);
        write_field("nullable", column.nullable);
        write_child_field("default_value", column.default_value.get());
        write_optional_field("comment", column.comment);
    }
}

void AstDebugPrinter::visit_create_index_statement(
    const CreateIndexStatement & statement
)
{
    write_node_header("CreateIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("index_name", statement.index_name());
    write_field("collection_name", statement.collection_name());
    write_field("column_name", statement.column_name());
    write_field("if_not_exists", statement.if_not_exists());
    write_field("method", create_index_method_name(statement.method()));
}

void AstDebugPrinter::visit_create_vector_index_statement(
    const CreateVectorIndexStatement & statement
)
{
    write_node_header("CreateVectorIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("index_name", statement.index_name());
    write_field("collection_name", statement.collection_name());
    write_field("column_name", statement.column_name());
    write_field("if_not_exists", statement.if_not_exists());
    write_field("method", create_vector_index_method_name(statement.method()));
    write_indent();
    ostream_ << "options:\n";
    {
        IndentScope options_scope(*this);
        write_field("metric", vector_index_metric_name(statement.options().metric));
        write_optional_field("max_neighbors", statement.options().max_neighbors);
        write_optional_field("ef_construction", statement.options().ef_construction);
        write_optional_field("ef_search", statement.options().ef_search);
        write_optional_field("random_seed", statement.options().random_seed);
    }
}

void AstDebugPrinter::visit_delete_statement(const DeleteStatement & statement)
{
    write_node_header("DeleteStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection", statement.collection_name());
    write_child_field("where", statement.where());
}

void AstDebugPrinter::visit_describe_collection_statement(
    const DescribeCollectionStatement & statement
)
{
    write_node_header("DescribeCollectionStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection_name", statement.collection_name());
}

void AstDebugPrinter::visit_drop_database_statement(
    const DropDatabaseStatement & statement
)
{
    write_node_header("DropDatabaseStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_name", statement.database_name());
    write_field("if_exists", statement.if_exists());
}

void AstDebugPrinter::visit_drop_collection_statement(
    const DropCollectionStatement & statement
)
{
    write_node_header("DropCollectionStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection_name", statement.collection_name());
    write_field("if_exists", statement.if_exists());
}

void AstDebugPrinter::visit_drop_index_statement(
    const DropIndexStatement & statement
)
{
    write_node_header("DropIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("index_name", statement.index_name());
    write_field("collection_name", statement.collection_name());
    write_field("if_exists", statement.if_exists());
}

void AstDebugPrinter::visit_drop_vector_index_statement(
    const DropVectorIndexStatement & statement
)
{
    write_node_header("DropVectorIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("vector_index_name", statement.vector_index_name());
    write_field("collection_name", statement.collection_name());
    write_field("if_exists", statement.if_exists());
}

void AstDebugPrinter::visit_insert_statement(const InsertStatement & statement)
{
    write_node_header("InsertStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection", statement.collection_name());

    write_indent();
    ostream_ << "columns:";
    if (statement.columns().empty()) {
        ostream_ << " []\n";
    } else {
        ostream_ << '\n';
        IndentScope columns_scope(*this);
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            write_indent();
            ostream_ << '[' << index << "] " << statement.columns()[index] << '\n';
        }
    }

    write_indent();
    ostream_ << "values:";
    if (statement.values().empty()) {
        ostream_ << " []\n";
    } else {
        ostream_ << '\n';
        IndentScope values_scope(*this);
        for (std::size_t index = 0; index < statement.values().size(); ++index) {
            pending_str_ = "[" + std::to_string(index) + "] ";
            print(*statement.values()[index]);
        }
    }
}

void AstDebugPrinter::visit_select_statement(const SelectStatement & statement)
{
    write_node_header("SelectStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection", statement.collection_name());

    write_indent();
    ostream_ << "select_list:";
    if (statement.select_list().empty()) {
        ostream_ << " []\n";
    } else {
        ostream_ << '\n';
        IndentScope select_scope(*this);
        for (std::size_t index = 0; index < statement.select_list().size(); ++index) {
            pending_str_ = "[" + std::to_string(index) + "] ";
            print(*statement.select_list()[index]);
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
            const auto & item = statement.order_by()[index];
            write_indent();
            ostream_ << '[' << index << "] OrderByItem\n";
            IndentScope item_scope(*this);
            write_child_field("expression", item.expression.get());
            write_field("ascending", item.ascending);
        }
    }

    write_optional_field("limit", statement.limit());
    write_optional_field("offset", statement.offset());
}

void AstDebugPrinter::visit_show_databases_statement(
    const ShowDatabasesStatement & statement
)
{
    write_node_header("ShowDatabasesStatement", statement.location());
}

void AstDebugPrinter::visit_show_collections_statement(
    const ShowCollectionsStatement & statement
)
{
    write_node_header("ShowCollectionsStatement", statement.location());
    IndentScope scope(*this);
    write_optional_field("database_name", statement.database_name());
}

void AstDebugPrinter::visit_show_indexes_statement(
    const ShowIndexesStatement & statement
)
{
    write_node_header("ShowIndexesStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection_name", statement.collection_name());
}

void AstDebugPrinter::visit_show_vector_indexes_statement(
    const ShowVectorIndexesStatement & statement
)
{
    write_node_header("ShowVectorIndexesStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection_name", statement.collection_name());
}

void AstDebugPrinter::visit_update_statement(const UpdateStatement & statement)
{
    write_node_header("UpdateStatement", statement.location());
    IndentScope scope(*this);
    write_field("collection", statement.collection_name());

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
            ostream_ << '[' << index << "] Assignment\n";
            IndentScope assignment_scope(*this);
            write_field("column", assignment.column_name);
            write_child_field("value", assignment.value.get());
        }
    }

    write_child_field("where", statement.where());
}

void AstDebugPrinter::visit_use_statement(const UseStatement & statement)
{
    write_node_header("UseStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_name", statement.database_name());
}

void AstDebugPrinter::visit_identifier_expression(
    const IdentifierExpression & expression
)
{
    write_node_header("IdentifierExpression", expression.location());
    IndentScope scope(*this);
    write_field("name", expression.name());
}

void AstDebugPrinter::visit_wildcard_expression(
    const WildcardExpression & expression
)
{
    write_node_header("WildcardExpression", expression.location());
    IndentScope scope(*this);
    write_optional_field("qualifier", expression.qualifier());
}

void AstDebugPrinter::visit_literal_expression(
    const LiteralExpression & expression
)
{
    write_node_header("LiteralExpression", expression.location());
    IndentScope scope(*this);
    write_field("literal_type", token_type_name(expression.literal_type()));
    write_field("value", expression.value());
}

void AstDebugPrinter::visit_function_call_expression(
    const FunctionCallExpression & expression
)
{
    write_node_header("FunctionCallExpression", expression.location());
    IndentScope scope(*this);
    write_field("name", expression.name());

    write_indent();
    ostream_ << "arguments:";
    if (expression.arguments().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope arguments_scope(*this);
    for (std::size_t index = 0; index < expression.arguments().size(); ++index) {
        pending_str_ = "[" + std::to_string(index) + "] ";
        print(*expression.arguments()[index]);
    }
}

void AstDebugPrinter::visit_column_reference_expression(
    const ColumnReferenceExpression & expression
)
{
    write_node_header("ColumnReferenceExpression", expression.location());
    IndentScope scope(*this);
    write_optional_field("qualifier", expression.qualifier());
    write_field("column", expression.column_name());
}

void AstDebugPrinter::visit_vector_expression(
    const VectorExpression & expression
)
{
    write_node_header("VectorExpression", expression.location());
    IndentScope scope(*this);
    write_indent();
    ostream_ << "elements:";
    if (expression.elements().empty()) {
        ostream_ << " []\n";
        return;
    }

    ostream_ << '\n';
    IndentScope elements_scope(*this);
    for (std::size_t index = 0; index < expression.elements().size(); ++index) {
        pending_str_ = "[" + std::to_string(index) + "] ";
        print(*expression.elements()[index]);
    }
}

void AstDebugPrinter::visit_binary_expression(
    const BinaryExpression & expression
)
{
    write_node_header("BinaryExpression", expression.location());
    IndentScope scope(*this);
    write_field("op", token_type_name(expression.op()));
    write_child_field("left", &expression.left());
    write_child_field("right", &expression.right());
}

void AstDebugPrinter::visit_unary_expression(
    const UnaryExpression & expression
)
{
    write_node_header("UnaryExpression", expression.location());
    IndentScope scope(*this);
    write_field("op", token_type_name(expression.op()));
    write_child_field("operand", &expression.operand());
}

void AstDebugPrinter::visit_in_expression(const InExpression & expression)
{
    write_node_header("InExpression", expression.location());
    IndentScope scope(*this);
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
        pending_str_ = "[" + std::to_string(index) + "] ";
        print(*expression.values()[index]);
    }
}

void AstDebugPrinter::visit_between_expression(
    const BetweenExpression & expression
)
{
    write_node_header("BetweenExpression", expression.location());
    IndentScope scope(*this);
    write_child_field("expression", &expression.expression());
    write_child_field("lower", &expression.lower());
    write_child_field("upper", &expression.upper());
}

void AstDebugPrinter::visit_like_expression(const LikeExpression & expression)
{
    write_node_header("LikeExpression", expression.location());
    IndentScope scope(*this);
    write_child_field("expression", &expression.expression());
    write_child_field("pattern", &expression.pattern());
}

void AstDebugPrinter::visit_alias_expression(
    const AliasExpression & expression
)
{
    write_node_header("AliasExpression", expression.location());
    IndentScope scope(*this);
    write_child_field("expression", &expression.expression());
    write_field("alias", expression.alias());
}

void AstDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        ostream_ << "  ";
    }
}

void AstDebugPrinter::write_node_header(
    std::string_view name,
    AstNodeLocation location
)
{
    write_indent();
    if (!pending_str_.empty()) {
        ostream_ << pending_str_;
        pending_str_.clear();
    }

    ostream_ << name;
    if (options_.include_location) {
        ostream_ << " @" << location.line << ':' << location.column;
    }
    ostream_ << '\n';
}

void AstDebugPrinter::write_field(
    std::string_view name,
    std::string_view value
)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void AstDebugPrinter::write_field(std::string_view name, bool value)
{
    write_indent();
    ostream_ << name << ": " << (value ? "true" : "false") << '\n';
}

void AstDebugPrinter::write_field(std::string_view name, std::size_t value)
{
    write_indent();
    ostream_ << name << ": " << value << '\n';
}

void AstDebugPrinter::write_optional_field(
    std::string_view name,
    const std::optional<std::string> & value
)
{
    write_field(
        name,
        value ? std::string_view(*value) : std::string_view("<none>")
    );
}

void AstDebugPrinter::write_optional_field(
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

void AstDebugPrinter::write_child_field(
    std::string_view name,
    const ExpressionNode * expression
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

std::string debug_print(const AstNode & node, AstDebugPrinterOptions options)
{
    std::ostringstream stream;
    debug_print(stream, node, options);
    return stream.str();
}

std::string debug_print(
    const StatementNode & statement,
    AstDebugPrinterOptions options
)
{
    std::ostringstream stream;
    debug_print(stream, statement, options);
    return stream.str();
}

std::string debug_print(
    const ExpressionNode & expression,
    AstDebugPrinterOptions options
)
{
    std::ostringstream stream;
    debug_print(stream, expression, options);
    return stream.str();
}

void debug_print(
    std::ostream & ostream,
    const AstNode & node,
    AstDebugPrinterOptions options
)
{
    AstDebugPrinter printer(ostream, options);
    printer.print(node);
}

void debug_print(
    std::ostream & ostream,
    const StatementNode & statement,
    AstDebugPrinterOptions options
)
{
    AstDebugPrinter printer(ostream, options);
    printer.print(statement);
}

void debug_print(
    std::ostream & ostream,
    const ExpressionNode & expression,
    AstDebugPrinterOptions options
)
{
    AstDebugPrinter printer(ostream, options);
    printer.print(expression);
}

} // namespace litedb::core::parser::ast
