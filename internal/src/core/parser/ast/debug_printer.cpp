#include "core/parser/ast/debug_printer.hpp"

#include "core/parser/ast/expression/alias_expression.hpp"
#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/expression/identifier_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/alter_statement.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/show_collections_statement.hpp"
#include "core/parser/ast/statement/show_databases_statement.hpp"
#include "core/parser/ast/statement/show_indexes_statement.hpp"
#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"
#include "core/parser/token.hpp"

#include <sstream>

namespace litedb::core::parser::ast
{

namespace
{

/**
 * @brief 获取 Token 类型名称
 * @param type Token 类型
 * @return Token 类型名称
 */
const char * token_type_name(TokenType type) noexcept
{
    switch (type) {
        case TokenType::EoF: return "EoF";
        case TokenType::Select: return "Select";
        case TokenType::Create: return "Create";
        case TokenType::Insert: return "Insert";
        case TokenType::Delete: return "Delete";
        case TokenType::Update: return "Update";
        case TokenType::Drop: return "Drop";
        case TokenType::Use: return "Use";
        case TokenType::Alter: return "Alter";
        case TokenType::Show: return "Show";
        case TokenType::Describe: return "Describe";
        case TokenType::Desc: return "Desc";
        case TokenType::Database: return "Database";
        case TokenType::Collection: return "Collection";
        case TokenType::Index: return "Index";
        case TokenType::VIndex: return "VIndex";
        case TokenType::Databases: return "Databases";
        case TokenType::Collections: return "Collections";
        case TokenType::Indexes: return "Indexes";
        case TokenType::VIndexes: return "VIndexes";
        case TokenType::Group: return "Group";
        case TokenType::By: return "By";
        case TokenType::Having: return "Having";
        case TokenType::Order: return "Order";
        case TokenType::Asc: return "Asc";
        case TokenType::Limit: return "Limit";
        case TokenType::Offset: return "Offset";
        case TokenType::In: return "In";
        case TokenType::Between: return "Between";
        case TokenType::Like: return "Like";
        case TokenType::Add: return "Add";
        case TokenType::Modify: return "Modify";
        case TokenType::Rename: return "Rename";
        case TokenType::Column: return "Column";
        case TokenType::To: return "To";
        case TokenType::Primary: return "Primary";
        case TokenType::Key: return "Key";
        case TokenType::Unique: return "Unique";
        case TokenType::AutoIncrement: return "AutoIncrement";
        case TokenType::Default: return "Default";
        case TokenType::Comment: return "Comment";
        case TokenType::Using: return "Using";
        case TokenType::BTree: return "BTree";
        case TokenType::With: return "With";
        case TokenType::From: return "From";
        case TokenType::Where: return "Where";
        case TokenType::Into: return "Into";
        case TokenType::Values: return "Values";
        case TokenType::Set: return "Set";
        case TokenType::And: return "And";
        case TokenType::Or: return "Or";
        case TokenType::Not: return "Not";
        case TokenType::As: return "As";
        case TokenType::On: return "On";
        case TokenType::If: return "If";
        case TokenType::Exists: return "Exists";
        case TokenType::Is: return "Is";
        case TokenType::Null: return "Null";
        case TokenType::True: return "True";
        case TokenType::False: return "False";
        case TokenType::Integer: return "Integer";
        case TokenType::BigInt: return "BigInt";
        case TokenType::Float: return "Float";
        case TokenType::Double: return "Double";
        case TokenType::Varchar: return "Varchar";
        case TokenType::Boolean: return "Boolean";
        case TokenType::Vector: return "Vector";
        case TokenType::Identifier: return "Identifier";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::IntegerLiteral: return "IntegerLiteral";
        case TokenType::FloatLiteral: return "FloatLiteral";
        case TokenType::Equal: return "Equal";
        case TokenType::NotEqual: return "NotEqual";
        case TokenType::LessThan: return "LessThan";
        case TokenType::GreaterThan: return "GreaterThan";
        case TokenType::LessEqual: return "LessEqual";
        case TokenType::GreaterEqual: return "GreaterEqual";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Star: return "Star";
        case TokenType::Slash: return "Slash";
        case TokenType::Modulo: return "Modulo";
        case TokenType::Comma: return "Comma";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Dot: return "Dot";
        case TokenType::LeftParen: return "LeftParen";
        case TokenType::RightParen: return "RightParen";
        case TokenType::LeftBracket: return "LeftBracket";
        case TokenType::RightBracket: return "RightBracket";
        case TokenType::Error: return "Error";
    }

    return "Unknown";
}

/**
 * @brief 获取 Schema 对象类型名称
 * @param type Schema 对象类型
 * @return Schema 对象类型名称
 */
const char * schema_object_type_name(SchemaObjectType type) noexcept
{
    switch (type) {
        case SchemaObjectType::Database: return "Database";
        case SchemaObjectType::Collection: return "Collection";
    }

    return "Unknown";
}

/**
 * @brief 获取数据类型类型名称
 * @param kind 数据类型类型
 * @return 数据类型类型名称
 */
const char * data_type_kind_name(DataTypeKind kind) noexcept
{
    switch (kind) {
        case DataTypeKind::Integer: return "Integer";
        case DataTypeKind::BigInt: return "BigInt";
        case DataTypeKind::Float: return "Float";
        case DataTypeKind::Double: return "Double";
        case DataTypeKind::Varchar: return "Varchar";
        case DataTypeKind::Boolean: return "Boolean";
        case DataTypeKind::Vector: return "Vector";
    }

    return "Unknown";
}

/**
 * @brief 获取创建索引方法名称
 * @param method 创建索引方法
 * @return 创建索引方法名称
 */
const char * create_index_method_name(CreateIndexMethod method) noexcept
{
    switch (method) {
        case CreateIndexMethod::Default: return "Default";
        case CreateIndexMethod::BTree: return "BTree";
    }

    return "Unknown";
}

/**
 * @brief 获取创建向量索引方法名称
 * @param method 创建向量索引方法
 * @return 创建向量索引方法名称
 */
const char * create_vector_index_method_name(CreateVectorIndexMethod method) noexcept
{
    switch (method) {
        case CreateVectorIndexMethod::Hnsw: return "Hnsw";
    }

    return "Unknown";
}

/**
 * @brief 获取向量索引指标名称
 * @param metric 向量索引指标
 * @return 向量索引指标名称
 */
const char * vector_index_metric_name(VectorIndexMetric metric) noexcept
{
    switch (metric) {
        case VectorIndexMetric::Default: return "Default";
        case VectorIndexMetric::L2: return "L2";
        case VectorIndexMetric::InnerProduct: return "InnerProduct";
        case VectorIndexMetric::Cosine: return "Cosine";
    }

    return "Unknown";
}

} // namespace

/**
 * @brief 缩进作用域
 * @details 当它被构造时，会递增调试打印器的缩进，当它被析构时，会递减调试打印器的缩进。
 */
class AstDebugPrinter::IndentScope
{
public:
    explicit IndentScope(AstDebugPrinter & printer) noexcept
        : printer_(printer)
    {
        printer_.indent_ += 1;
    }

    ~IndentScope() noexcept
    {
        printer_.indent_ -= 1;
    }

private:
    AstDebugPrinter & printer_;         ///< 调试打印器
};

AstDebugPrinter::AstDebugPrinter(std::ostream & out, AstDebugPrinterOptions options)
    : out_(out)
    , options_(options)
{
}

void AstDebugPrinter::print(const AstNode & node)
{
    node.accept(*this);
}

void AstDebugPrinter::write_indent()
{
    for (std::size_t index = 0; index < indent_; ++index) {
        out_ << "  ";
    }
}

void AstDebugPrinter::write_node_header(const char * name, AstNodeLocation location)
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

void AstDebugPrinter::write_field(const char * name, const std::string & value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void AstDebugPrinter::write_field(const char * name, const char * value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void AstDebugPrinter::write_field(const char * name, bool value)
{
    write_indent();
    out_ << name << ": " << (value ? "true" : "false") << '\n';
}

void AstDebugPrinter::write_field(const char * name, std::size_t value)
{
    write_indent();
    out_ << name << ": " << value << '\n';
}

void AstDebugPrinter::write_optional_field(const char * name, const std::optional<std::size_t> & value)
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

void AstDebugPrinter::write_child_field(const char * name, const AstNode * node)
{
    write_indent();
    out_ << name << ':';
    if (node == nullptr) {
        out_ << " <none>\n";
        return;
    }

    out_ << '\n';
    IndentScope scope(*this);
    node->accept(*this);
}

void AstDebugPrinter::visit(const AlterStatement & node)
{
    write_node_header("AlterStatement", node.location());
    IndentScope scope(*this);
    write_field("object_type", schema_object_type_name(node.object_type()));
    write_field("name", node.name());
}

void AstDebugPrinter::visit(const CreateCollectionStatement & node)
{
    write_node_header("CreateCollectionStatement", node.location());
    IndentScope scope(*this);
    write_field("collection", node.collection());
    write_field("if_not_exists", node.if_not_exists());
    if (node.comment().has_value()) {
        write_field("comment", node.comment().value());
    } else {
        write_field("comment", "<none>");
    }
    write_indent();
    out_ << "columns:";
    if (node.columns().empty()) {
        out_ << " []\n";
        return;
    }

    out_ << '\n';
    IndentScope columns_scope(*this);
    for (std::size_t index = 0; index < node.columns().size(); ++index) {
        const auto & column = node.columns()[index];
        write_indent();
        out_ << '[' << index << "] ColumnDefinition\n";
        IndentScope column_scope(*this);
        write_field("name", column.name);
        write_indent();
        out_ << "type:\n";
        {
            IndentScope type_scope(*this);
            write_field("kind", data_type_kind_name(column.type.kind));
            write_optional_field("parameter", column.type.parameter);
        }
        write_field("unique", column.unique);
        write_field("nullable", column.nullable);
        write_child_field("default_value", column.default_value.get());
        if (column.comment.has_value()) {
            write_field("comment", column.comment.value());
        } else {
            write_field("comment", "<none>");
        }
    }
}

void AstDebugPrinter::visit(const CreateDatabaseStatement & node)
{
    write_node_header("CreateDatabaseStatement", node.location());
    IndentScope scope(*this);
    write_field("database", node.database());
    write_field("if_not_exists", node.if_not_exists());
}

void AstDebugPrinter::visit(const CreateIndexStatement & node)
{
    write_node_header("CreateIndexStatement", node.location());
    IndentScope scope(*this);
    write_field("index_name", node.index_name());
    write_field("collection_name", node.collection_name());
    write_field("column_name", node.column_name());
    write_field("if_not_exists", node.if_not_exists());
    write_field("method", create_index_method_name(node.method()));
}

void AstDebugPrinter::visit(const CreateVectorIndexStatement & node)
{
    write_node_header("CreateVectorIndexStatement", node.location());
    IndentScope scope(*this);
    write_field("index_name", node.index_name());
    write_field("collection_name", node.collection_name());
    write_field("column_name", node.column_name());
    write_field("if_not_exists", node.if_not_exists());
    write_field("method", create_vector_index_method_name(node.method()));
    write_indent();
    out_ << "options:\n";
    {
        IndentScope options_scope(*this);
        write_field("metric", vector_index_metric_name(node.options().metric));
        write_optional_field("max_neighbors", node.options().max_neighbors);
        write_optional_field("ef_construction", node.options().ef_construction);
        write_optional_field("ef_search", node.options().ef_search);
        write_optional_field("random_seed", node.options().random_seed);
    }
}

void AstDebugPrinter::visit(const DeleteStatement & node)
{
    write_node_header("DeleteStatement", node.location());
    IndentScope scope(*this);
    write_field("collection", node.collection());
    write_child_field("where", node.where());
}

void AstDebugPrinter::visit(const DescribeStatement & node)
{
    write_node_header("DescribeStatement", node.location());
    IndentScope scope(*this);
    write_field("object_type", schema_object_type_name(node.object_type()));
    write_field("name", node.name());
}

void AstDebugPrinter::visit(const DropCollectionStatement & node)
{
    write_node_header("DropCollectionStatement", node.location());
    IndentScope scope(*this);
    write_field("collection_name", node.collection_name());
    write_field("if_exists", node.if_exists());
}

void AstDebugPrinter::visit(const DropDatabaseStatement & node)
{
    write_node_header("DropDatabaseStatement", node.location());
    IndentScope scope(*this);
    write_field("database_name", node.database_name());
    write_field("if_exists", node.if_exists());
}

void AstDebugPrinter::visit(const DropIndexStatement & node)
{
    write_node_header("DropIndexStatement", node.location());
    IndentScope scope(*this);
    write_field("index_name", node.index_name());
    write_field("collection_name", node.collection_name());
    write_field("if_exists", node.if_exists());
}

void AstDebugPrinter::visit(const DropVectorIndexStatement & node)
{
    write_node_header("DropVectorIndexStatement", node.location());
    IndentScope scope(*this);
    write_field("index_name", node.index_name());
    write_field("collection_name", node.collection_name());
    write_field("if_exists", node.if_exists());
}

void AstDebugPrinter::visit(const InsertStatement & node)
{
    write_node_header("InsertStatement", node.location());
    IndentScope scope(*this);
    write_field("collection", node.collection());
    write_indent();
    out_ << "columns:";
    if (node.columns().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope columns_scope(*this);
        for (std::size_t index = 0; index < node.columns().size(); ++index) {
            write_indent();
            out_ << '[' << index << "] " << node.columns()[index] << '\n';
        }
    }

    write_indent();
    out_ << "values:";
    if (node.values().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope values_scope(*this);
        for (std::size_t index = 0; index < node.values().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            node.values()[index]->accept(*this);
        }
    }
}

void AstDebugPrinter::visit(const SelectStatement & node)
{
    write_node_header("SelectStatement", node.location());
    IndentScope scope(*this);
    write_field("collection", node.collection());
    write_indent();
    out_ << "select_list:";
    if (node.select_list().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope select_scope(*this);
        for (std::size_t index = 0; index < node.select_list().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            node.select_list()[index]->accept(*this);
        }
    }

    write_child_field("where", node.where());

    write_indent();
    out_ << "order_by:";
    if (node.order_by().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope order_scope(*this);
        for (std::size_t index = 0; index < node.order_by().size(); ++index) {
            write_indent();
            out_ << '[' << index << "] OrderByItem\n";
            IndentScope item_scope(*this);
            write_child_field("expression", node.order_by()[index].expression.get());
            write_field("ascending", node.order_by()[index].ascending);
        }
    }

    write_optional_field("limit", node.limit());
    write_optional_field("offset", node.offset());
}

void AstDebugPrinter::visit(const ShowCollectionsStatement & node)
{
    write_node_header("ShowCollectionsStatement", node.location());
    IndentScope scope(*this);
    if (node.database_name().has_value()) {
        write_field("database_name", node.database_name().value());
    } else {
        write_field("database_name", "null");
    }
}

void AstDebugPrinter::visit(const ShowDatabasesStatement & node)
{
    write_node_header("ShowDatabasesStatement", node.location());
}

void AstDebugPrinter::visit(const ShowIndexesStatement & node)
{
    write_node_header("ShowIndexesStatement", node.location());
    IndentScope scope(*this);
    write_field("collection_name", node.collection_name());
}

void AstDebugPrinter::visit(const ShowVectorIndexesStatement & node)
{
    write_node_header("ShowVectorIndexesStatement", node.location());
    IndentScope scope(*this);
    write_field("collection_name", node.collection_name());
}

void AstDebugPrinter::visit(const UpdateStatement & node)
{
    write_node_header("UpdateStatement", node.location());
    IndentScope scope(*this);
    write_field("collection", node.collection());
    write_indent();
    out_ << "assignments:";
    if (node.assignments().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope assignments_scope(*this);
        for (std::size_t index = 0; index < node.assignments().size(); ++index) {
            write_indent();
            out_ << '[' << index << "] Assignment\n";
            IndentScope assignment_scope(*this);
            write_field("column", node.assignments()[index].column);
            write_child_field("value", node.assignments()[index].value.get());
        }
    }
    write_child_field("where", node.where());
}

void AstDebugPrinter::visit(const UseStatement & node)
{
    write_node_header("UseStatement", node.location());
    IndentScope scope(*this);
    write_field("database", node.database());
}

void AstDebugPrinter::visit(const AliasExpression & node)
{
    write_node_header("AliasExpression", node.location());
    IndentScope scope(*this);
    write_child_field("expression", &node.expression());
    write_field("alias", node.alias());
}

void AstDebugPrinter::visit(const BetweenExpression & node)
{
    write_node_header("BetweenExpression", node.location());
    IndentScope scope(*this);
    write_child_field("expression", &node.expression());
    write_child_field("lower", &node.lower());
    write_child_field("upper", &node.upper());
}

void AstDebugPrinter::visit(const BinaryExpression & node)
{
    write_node_header("BinaryExpression", node.location());
    IndentScope scope(*this);
    write_field("op", token_type_name(node.op()));
    write_child_field("left", &node.left());
    write_child_field("right", &node.right());
}

void AstDebugPrinter::visit(const ColumnReferenceExpression & node)
{
    write_node_header("ColumnReferenceExpression", node.location());
    IndentScope scope(*this);
    if (node.qualifier().has_value()) {
        write_field("qualifier", node.qualifier().value());
    } else {
        write_field("qualifier", "<none>");
    }
    write_field("column", node.column());
}

void AstDebugPrinter::visit(const FunctionCallExpression & node)
{
    write_node_header("FunctionCallExpression", node.location());
    IndentScope scope(*this);
    write_field("name", node.name());
    write_indent();
    out_ << "arguments:";
    if (node.arguments().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope arguments_scope(*this);
        for (std::size_t index = 0; index < node.arguments().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            node.arguments()[index]->accept(*this);
        }
    }
}

void AstDebugPrinter::visit(const IdentifierExpression & node)
{
    write_node_header("IdentifierExpression", node.location());
    IndentScope scope(*this);
    write_field("name", node.name());
}

void AstDebugPrinter::visit(const InExpression & node)
{
    write_node_header("InExpression", node.location());
    IndentScope scope(*this);
    write_child_field("expression", &node.expression());
    write_indent();
    out_ << "values:";
    if (node.values().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope values_scope(*this);
        for (std::size_t index = 0; index < node.values().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            node.values()[index]->accept(*this);
        }
    }
}

void AstDebugPrinter::visit(const LikeExpression & node)
{
    write_node_header("LikeExpression", node.location());
    IndentScope scope(*this);
    write_child_field("expression", &node.expression());
    write_child_field("pattern", &node.pattern());
}

void AstDebugPrinter::visit(const LiteralExpression & node)
{
    write_node_header("LiteralExpression", node.location());
    IndentScope scope(*this);
    write_field("literal_type", token_type_name(node.literal_type()));
    write_field("value", node.value());
}

void AstDebugPrinter::visit(const UnaryExpression & node)
{
    write_node_header("UnaryExpression", node.location());
    IndentScope scope(*this);
    write_field("op", token_type_name(node.op()));
    write_child_field("operand", &node.operand());
}

void AstDebugPrinter::visit(const VectorExpression & node)
{
    write_node_header("VectorExpression", node.location());
    IndentScope scope(*this);
    write_indent();
    out_ << "elements:";
    if (node.elements().empty()) {
        out_ << " []\n";
    } else {
        out_ << '\n';
        IndentScope elements_scope(*this);
        for (std::size_t index = 0; index < node.elements().size(); ++index) {
            pending_prefix_ = "[" + std::to_string(index) + "] ";
            node.elements()[index]->accept(*this);
        }
    }
}

void AstDebugPrinter::visit(const WildcardExpression & node)
{
    write_node_header("WildcardExpression", node.location());
    IndentScope scope(*this);
    if (node.qualifier().has_value()) {
        write_field("qualifier", node.qualifier().value());
    } else {
        write_field("qualifier", "<none>");
    }
}

std::string debug_print(const AstNode & node, AstDebugPrinterOptions options)
{
    std::ostringstream out;
    debug_print(out, node, options);
    return out.str();
}

void debug_print(std::ostream & out, const AstNode & node, AstDebugPrinterOptions options)
{
    AstDebugPrinter printer(out, options);
    printer.print(node);
}

} // namespace litedb::core::parser::ast
