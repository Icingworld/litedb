#include "core/binder/bound/debug_printer.hpp"

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/binder/bound/expression/bound_wildcard_expression.hpp"
#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/meta/meta.hpp"
#include "core/parser/token.hpp"

#include <sstream>

namespace litedb::core::binder::bound
{

namespace
{

const char * logical_type_name(common::LogicalTypeId id) noexcept
{
    switch (id) {
    case common::LogicalTypeId::Null: return "NULL";
    case common::LogicalTypeId::Boolean: return "BOOLEAN";
    case common::LogicalTypeId::Integer: return "INTEGER";
    case common::LogicalTypeId::BigInt: return "BIGINT";
    case common::LogicalTypeId::Float: return "FLOAT";
    case common::LogicalTypeId::Double: return "DOUBLE";
    case common::LogicalTypeId::Varchar: return "VARCHAR";
    case common::LogicalTypeId::Vector: return "VECTOR";
    }

    return "UNKNOWN";
}

std::string logical_type_text(const common::LogicalType & type)
{
    std::string text {logical_type_name(type.id)};
    if (type.parameter.has_value()) {
        text += "(" + std::to_string(type.parameter.value()) + ")";
    }
    return text;
}

const char * token_type_name(parser::TokenType type) noexcept
{
    switch (type) {
    case parser::TokenType::Equal: return "Equal";
    case parser::TokenType::NotEqual: return "NotEqual";
    case parser::TokenType::LessThan: return "LessThan";
    case parser::TokenType::GreaterThan: return "GreaterThan";
    case parser::TokenType::LessEqual: return "LessEqual";
    case parser::TokenType::GreaterEqual: return "GreaterEqual";
    case parser::TokenType::Plus: return "Plus";
    case parser::TokenType::Minus: return "Minus";
    case parser::TokenType::Star: return "Star";
    case parser::TokenType::Slash: return "Slash";
    case parser::TokenType::Modulo: return "Modulo";
    case parser::TokenType::And: return "And";
    case parser::TokenType::Or: return "Or";
    case parser::TokenType::Not: return "Not";
    default: return "Unknown";
    }
}

const char * meta_index_kind_name(meta::entry::IndexKind kind) noexcept
{
    switch (kind) {
    case meta::entry::IndexKind::BTree: return "BTree";
    }

    return "Unknown";
}

const char * meta_vector_index_kind_name(meta::entry::VectorIndexKind kind) noexcept
{
    switch (kind) {
    case meta::entry::VectorIndexKind::Hnsw: return "Hnsw";
    }

    return "Unknown";
}

const char * meta_vector_metric_name(meta::entry::VectorDistanceMetric metric) noexcept
{
    switch (metric) {
    case meta::entry::VectorDistanceMetric::L2: return "L2";
    case meta::entry::VectorDistanceMetric::InnerProduct: return "InnerProduct";
    case meta::entry::VectorDistanceMetric::Cosine: return "Cosine";
    }

    return "Unknown";
}

} // namespace

class BoundDebugPrinter::IndentScope
{
public:
    explicit IndentScope(BoundDebugPrinter & printer) noexcept
        : printer_(printer)
    {
        printer_.indent_ += 1;
    }

    ~IndentScope() noexcept
    {
        printer_.indent_ -= 1;
    }

private:
    BoundDebugPrinter & printer_;
};

BoundDebugPrinter::BoundDebugPrinter(std::ostream & out, BoundDebugPrinterOptions options)
    : out_(out)
    , options_(options)
{
}

void BoundDebugPrinter::print(const BoundStatement & statement)
{
    statement.accept(*this);
}

void BoundDebugPrinter::print(const BoundExpression & expression)
{
    expression.accept(*this);
}

void BoundDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        out_ << "  ";
    }
}

void BoundDebugPrinter::write_node_header(const char * name, parser::ast::AstNodeLocation location)
{
    write_indent();
    if (!pending_prefix_.empty()) {
        out_ << pending_prefix_;
        pending_prefix_.clear();
    }
    out_ << name;
    if (options_.include_location) {
        out_ << " @" << location.line << ':' << location.column;
    }
    out_ << '\n';
}

void BoundDebugPrinter::write_field(const char * name, const std::string & value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void BoundDebugPrinter::write_field(const char * name, const char * value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void BoundDebugPrinter::write_field(const char * name, bool value)
{
    write_indent();
    out_ << name << ": " << (value ? "true" : "false") << '\n';
}

void BoundDebugPrinter::write_field(const char * name, std::size_t value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void BoundDebugPrinter::write_optional_field(const char * name, std::optional<std::size_t> value)
{
    write_indent();
    out_ << name << ": ";
    if (value.has_value()) {
        out_ << value.value();
    } else {
        out_ << "<none>";
    }
    out_ << '\n';
}

void BoundDebugPrinter::write_type_field(const char * name, const common::LogicalType & type)
{
    if (!options_.include_type) {
        return;
    }
    write_field(name, logical_type_text(type));
}

void BoundDebugPrinter::write_expression_header(const char * name, const BoundExpression & expression)
{
    write_node_header(name, expression.location());
}

void BoundDebugPrinter::write_child_field(const char * name, const BoundExpression * expression)
{
    write_indent();
    out_ << name << ':';
    if (expression == nullptr) {
        out_ << " <none>\n";
        return;
    }

    out_ << '\n';
    IndentScope scope(*this);
    expression->accept(*this);
}

void BoundDebugPrinter::write_bound_column(const BoundColumn & column)
{
    write_indent();
    if (!pending_prefix_.empty()) {
        out_ << pending_prefix_;
        pending_prefix_.clear();
    }
    out_ << "BoundColumn\n";
    IndentScope scope(*this);
    write_field("column_id", column.column_id);
    write_field("name", column.name);
    write_type_field("type", column.type);
    write_field("nullable", column.nullable);
}

void BoundDebugPrinter::visit(const BoundCreateDatabaseStatement & statement)
{
    write_node_header("BoundCreateDatabaseStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_name", statement.database_name());
    write_field("if_not_exists", statement.if_not_exists());
}

void BoundDebugPrinter::visit(const BoundCreateCollectionStatement & statement)
{
    write_node_header("BoundCreateCollectionStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_name", statement.collection_name());
    write_field("if_not_exists", statement.if_not_exists());
    if (statement.comment().has_value()) {
        write_field("comment", statement.comment().value());
    } else {
        write_field("comment", "<none>");
    }

    write_indent();
    out_ << "columns:";
    if (statement.columns().empty()) {
        out_ << " []\n";
        return;
    }
    out_ << '\n';
    IndentScope columns_scope(*this);
    for (std::size_t index = 0; index < statement.columns().size(); ++index) {
        const auto & column = statement.columns()[index];
        write_indent();
        out_ << '[' << index << "] ColumnDefinition\n";
        IndentScope column_scope(*this);
        write_field("name", column.name);
        write_type_field("type", column.type);
        write_field("unique", column.unique);
        write_field("nullable", column.nullable);
        write_field("default_expression", column.default_expression.has_value() ? "<present>" : "<none>");
        if (column.comment.has_value()) {
            write_field("comment", column.comment.value());
        } else {
            write_field("comment", "<none>");
        }
    }
}

void BoundDebugPrinter::visit(const BoundCreateIndexStatement & statement)
{
    write_node_header("BoundCreateIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
    write_field("column_id", statement.column_id());
    write_field("column_name", statement.column_name());
    write_field("index_name", statement.index_name());
    write_field("index_kind", meta_index_kind_name(statement.index_kind()));
    write_field("unique", statement.unique());
    write_field("if_not_exists", statement.if_not_exists());
}

void BoundDebugPrinter::visit(const BoundCreateVectorIndexStatement & statement)
{
    write_node_header("BoundCreateVectorIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
    write_field("column_id", statement.column_id());
    write_field("column_name", statement.column_name());
    write_field("index_name", statement.index_name());
    write_field("index_kind", meta_vector_index_kind_name(statement.index_kind()));
    write_field("metric", meta_vector_metric_name(statement.metric()));
    write_field("max_neighbors", statement.max_neighbors());
    write_field("ef_construction", statement.ef_construction());
    write_field("ef_search_default", statement.ef_search_default());
    write_field("random_seed", statement.random_seed());
    write_field("if_not_exists", statement.if_not_exists());
}

void BoundDebugPrinter::visit(const BoundDeleteStatement & statement)
{
    write_node_header("BoundDeleteStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
    write_child_field("where", statement.where());
}

void BoundDebugPrinter::visit(const BoundDescribeCollectionStatement & statement)
{
    write_node_header("BoundDescribeCollectionStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
}

void BoundDebugPrinter::visit(const BoundDropDatabaseStatement & statement)
{
    write_node_header("BoundDropDatabaseStatement", statement.location());
    IndentScope scope(*this);
    write_optional_field("database_id", statement.database_id());
    write_field("database_name", statement.database_name());
    write_field("if_exists", statement.if_exists());
}

void BoundDebugPrinter::visit(const BoundDropCollectionStatement & statement)
{
    write_node_header("BoundDropCollectionStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_optional_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
    write_field("if_exists", statement.if_exists());
}

void BoundDebugPrinter::visit(const BoundDropIndexStatement & statement)
{
    write_node_header("BoundDropIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
    write_field("index_name", statement.index_name());
    write_field("if_exists", statement.if_exists());
}

void BoundDebugPrinter::visit(const BoundDropVectorIndexStatement & statement)
{
    write_node_header("BoundDropVectorIndexStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
    write_field("index_name", statement.index_name());
    write_field("if_exists", statement.if_exists());
}

void BoundDebugPrinter::visit(const BoundInsertStatement & statement)
{
    write_node_header("BoundInsertStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());

    write_indent();
    out_ << "columns:";
    if (statement.columns().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope columns_scope(*this);
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            write_bound_column(statement.columns()[index]);
        }
    }

    write_indent();
    out_ << "values:";
    if (statement.values().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope values_scope(*this);
        for (std::size_t index = 0; index < statement.values().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            statement.values()[index]->accept(*this);
        }
    }
}

void BoundDebugPrinter::visit(const BoundSelectStatement & statement)
{
    write_node_header("BoundSelectStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());

    write_indent();
    out_ << "projections:";
    if (statement.projections().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope projections_scope(*this);
        for (std::size_t index = 0; index < statement.projections().size(); ++index) {
            write_indent();
            out_ << '[' << index << "] BoundProjectionItem\n";
            IndentScope item_scope(*this);
            if (statement.projections()[index].alias.has_value()) {
                write_field("alias", statement.projections()[index].alias.value());
            } else {
                write_field("alias", "<none>");
            }
            write_child_field("expression", statement.projections()[index].expression.get());
        }
    }

    write_child_field("where", statement.where());

    write_indent();
    out_ << "order_by:";
    if (statement.order_by().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope order_scope(*this);
        for (std::size_t index = 0; index < statement.order_by().size(); ++index) {
            write_indent();
            out_ << '[' << index << "] BoundOrderByItem\n";
            IndentScope item_scope(*this);
            write_child_field("expression", statement.order_by()[index].expression.get());
            write_field("ascending", statement.order_by()[index].ascending);
        }
    }

    write_optional_field("limit", statement.limit());
    write_optional_field("offset", statement.offset());
}

void BoundDebugPrinter::visit(const BoundShowDatabasesStatement & statement)
{
    write_node_header("BoundShowDatabasesStatement", statement.location());
}

void BoundDebugPrinter::visit(const BoundShowCollectionsStatement & statement)
{
    write_node_header("BoundShowCollectionsStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
}

void BoundDebugPrinter::visit(const BoundShowIndexesStatement & statement)
{
    write_node_header("BoundShowIndexesStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
}

void BoundDebugPrinter::visit(const BoundShowVectorIndexesStatement & statement)
{
    write_node_header("BoundShowVectorIndexesStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());
}

void BoundDebugPrinter::visit(const BoundUpdateStatement & statement)
{
    write_node_header("BoundUpdateStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("collection_id", statement.collection_id());
    write_field("collection_name", statement.collection_name());

    write_indent();
    out_ << "assignments:";
    if (statement.assignments().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope assignments_scope(*this);
        for (std::size_t index = 0; index < statement.assignments().size(); ++index) {
            write_indent();
            out_ << '[' << index << "] BoundAssignment\n";
            IndentScope assignment_scope(*this);
            write_indent();
            out_ << "column:\n";
            {
                IndentScope column_scope(*this);
                write_bound_column(statement.assignments()[index].column);
            }
            write_child_field("value", statement.assignments()[index].value.get());
        }
    }
    write_child_field("where", statement.where());
}

void BoundDebugPrinter::visit(const BoundUseStatement & statement)
{
    write_node_header("BoundUseStatement", statement.location());
    IndentScope scope(*this);
    write_field("database_id", statement.database_id());
    write_field("database_name", statement.database_name());
}

void BoundDebugPrinter::visit(const BoundBetweenExpression & expression)
{
    write_expression_header("BoundBetweenExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_child_field("expression", &expression.expression());
    write_child_field("lower", &expression.lower());
    write_child_field("upper", &expression.upper());
}

void BoundDebugPrinter::visit(const BoundBinaryExpression & expression)
{
    write_expression_header("BoundBinaryExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("op", token_type_name(expression.op()));
    write_child_field("left", &expression.left());
    write_child_field("right", &expression.right());
}

void BoundDebugPrinter::visit(const BoundCastExpression & expression)
{
    write_expression_header("BoundCastExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_type_field("target_type", expression.type());
    write_child_field("expression", &expression.expression());
}

void BoundDebugPrinter::visit(const BoundColumnRefExpression & expression)
{
    write_expression_header("BoundColumnRefExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("database_id", expression.database_id());
    write_field("collection_id", expression.collection_id());
    write_field("collection_name", expression.collection_name());
    write_field("column_id", expression.column_id());
    write_field("column_name", expression.column_name());
    write_field("nullable", expression.nullable());
}

void BoundDebugPrinter::visit(const BoundFunctionExpression & expression)
{
    write_expression_header("BoundFunctionExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("name", expression.name());
    write_indent();
    out_ << "signature:\n";
    {
        IndentScope signature_scope(*this);
        write_field("name", expression.signature().name);
        write_indent();
        out_ << "argument_types:";
        if (expression.signature().argument_types.empty()) {
            out_ << " []\n";
        } else {
            out_ << '\n';
            IndentScope arguments_scope(*this);
            for (std::size_t index = 0; index < expression.signature().argument_types.size(); ++index) {
                write_indent();
                out_ << '[' << index << "] " << logical_type_text(expression.signature().argument_types[index]) << '\n';
            }
        }
        write_type_field("return_type", expression.signature().return_type);
        write_field("variadic", expression.signature().variadic);
    }

    write_indent();
    out_ << "arguments:";
    if (expression.arguments().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope arguments_scope(*this);
        for (std::size_t index = 0; index < expression.arguments().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            expression.arguments()[index]->accept(*this);
        }
    }
}

void BoundDebugPrinter::visit(const BoundInExpression & expression)
{
    write_expression_header("BoundInExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_child_field("expression", &expression.expression());
    write_indent();
    out_ << "values:";
    if (expression.values().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope values_scope(*this);
        for (std::size_t index = 0; index < expression.values().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            expression.values()[index]->accept(*this);
        }
    }
}

void BoundDebugPrinter::visit(const BoundLikeExpression & expression)
{
    write_expression_header("BoundLikeExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_child_field("expression", &expression.expression());
    write_child_field("pattern", &expression.pattern());
}

void BoundDebugPrinter::visit(const BoundLiteralExpression & expression)
{
    write_expression_header("BoundLiteralExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("value", expression.value());
}

void BoundDebugPrinter::visit(const BoundNullExpression & expression)
{
    write_expression_header("BoundNullExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
}

void BoundDebugPrinter::visit(const BoundUnaryExpression & expression)
{
    write_expression_header("BoundUnaryExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_field("op", token_type_name(expression.op()));
    write_child_field("operand", &expression.operand());
}

void BoundDebugPrinter::visit(const BoundVectorExpression & expression)
{
    write_expression_header("BoundVectorExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    write_indent();
    out_ << "elements:";
    if (expression.elements().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope elements_scope(*this);
        for (std::size_t index = 0; index < expression.elements().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            expression.elements()[index]->accept(*this);
        }
    }
}

void BoundDebugPrinter::visit(const BoundWildcardExpression & expression)
{
    write_expression_header("BoundWildcardExpression", expression);
    IndentScope scope(*this);
    write_type_field("type", expression.type());
    if (expression.qualifier().has_value()) {
        write_field("qualifier", expression.qualifier().value());
    } else {
        write_field("qualifier", "<none>");
    }
}

std::string debug_print(const BoundStatement & statement, BoundDebugPrinterOptions options)
{
    std::ostringstream out;
    debug_print(out, statement, options);
    return out.str();
}

std::string debug_print(const BoundExpression & expression, BoundDebugPrinterOptions options)
{
    std::ostringstream out;
    debug_print(out, expression, options);
    return out.str();
}

void debug_print(std::ostream & out, const BoundStatement & statement, BoundDebugPrinterOptions options)
{
    BoundDebugPrinter printer(out, options);
    printer.print(statement);
}

void debug_print(std::ostream & out, const BoundExpression & expression, BoundDebugPrinterOptions options)
{
    BoundDebugPrinter printer(out, options);
    printer.print(expression);
}

} // namespace litedb::core::binder::bound
