#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/parser/ast/dispatcher/expression_dispatcher.hpp"
#include "core/parser/ast/dispatcher/statement_dispatcher.hpp"

namespace litedb::core::parser::ast
{

// AST 调试打印器选项
struct AstDebugPrinterOptions
{
    bool include_location {true};    // 是否包含位置信息
};

// AST 调试打印器
class AstDebugPrinter final
    : private ConstAstStatementDispatcher<AstDebugPrinter, void>
    , private ConstAstExpressionDispatcher<AstDebugPrinter, void>
{
    friend ConstAstStatementDispatcher<AstDebugPrinter, void>;
    friend ConstAstExpressionDispatcher<AstDebugPrinter, void>;

public:
    explicit AstDebugPrinter(
        std::ostream & ostream,
        AstDebugPrinterOptions options = {}
    );

public:
    // 打印 AST 节点
    void print(const AstNode & node);

    // 打印语句
    void print(const StatementNode & statement);

    // 打印表达式
    void print(const ExpressionNode & expression);

private:
    // 访问 CREATE DATABASE 语句
    void visit_create_database_statement(
        const CreateDatabaseStatement & statement
    );

    // 访问 CREATE COLLECTION 语句
    void visit_create_collection_statement(
        const CreateCollectionStatement & statement
    );

    // 访问 CREATE INDEX 语句
    void visit_create_index_statement(
        const CreateIndexStatement & statement
    );

    // 访问 CREATE VINDEX 语句
    void visit_create_vector_index_statement(
        const CreateVectorIndexStatement & statement
    );

    // 访问 DELETE 语句
    void visit_delete_statement(
        const DeleteStatement & statement
    );

    // 访问 DESCRIBE COLLECTION 语句
    void visit_describe_collection_statement(
        const DescribeCollectionStatement & statement
    );

    // 访问 DROP DATABASE 语句
    void visit_drop_database_statement(
        const DropDatabaseStatement & statement
    );

    // 访问 DROP COLLECTION 语句
    void visit_drop_collection_statement(
        const DropCollectionStatement & statement
    );

    // 访问 DROP INDEX 语句
    void visit_drop_index_statement(
        const DropIndexStatement & statement
    );

    // 访问 DROP VINDEX 语句
    void visit_drop_vector_index_statement(
        const DropVectorIndexStatement & statement
    );

    // 访问 INSERT 语句
    void visit_insert_statement(
        const InsertStatement & statement
    );

    // 访问 SELECT 语句
    void visit_select_statement(
        const SelectStatement & statement
    );

    // 访问 SHOW DATABASES 语句
    void visit_show_databases_statement(
        const ShowDatabasesStatement & statement
    );

    // 访问 SHOW COLLECTIONS 语句
    void visit_show_collections_statement(
        const ShowCollectionsStatement & statement
    );

    // 访问 SHOW INDEXES 语句
    void visit_show_indexes_statement(
        const ShowIndexesStatement & statement
    );

    // 访问 SHOW VINDEXES 语句
    void visit_show_vector_indexes_statement(
        const ShowVectorIndexesStatement & statement
    );

    // 访问 UPDATE 语句
    void visit_update_statement(
        const UpdateStatement & statement
    );

    // 访问 USE 语句
    void visit_use_statement(
        const UseStatement & statement
    );

    // 访问通配符表达式
    void visit_wildcard_expression(
        const WildcardExpression & expression
    );

    // 访问字面量表达式
    void visit_literal_expression(
        const LiteralExpression & expression
    );

    // 访问函数调用表达式
    void visit_function_call_expression(
        const FunctionCallExpression & expression
    );

    // 访问列引用表达式
    void visit_column_reference_expression(
        const ColumnReferenceExpression & expression
    );

    // 访问向量表达式
    void visit_vector_expression(
        const VectorExpression & expression
    );

    // 访问二元表达式
    void visit_binary_expression(
        const BinaryExpression & expression
    );

    // 访问一元表达式
    void visit_unary_expression(
        const UnaryExpression & expression
    );

    // 访问 IN 表达式
    void visit_in_expression(
        const InExpression & expression
    );

    // 访问 BETWEEN 表达式
    void visit_between_expression(
        const BetweenExpression & expression
    );

    // 访问 LIKE 表达式
    void visit_like_expression(
        const LikeExpression & expression
    );

    // 访问别名表达式
    void visit_alias_expression(
        const AliasExpression & expression
    );

    // 写入缩进
    void write_indent();

    // 写入节点头
    void write_node_header(std::string_view name, AstNodeLocation location);

    // 写入字段
    void write_field(std::string_view name, std::string_view value);

    // 写入字段
    void write_field(std::string_view name, bool value);

    // 写入字段
    void write_field(std::string_view name, std::size_t value);

    // 写入可选字段
    void write_optional_field(
        std::string_view name,
        const std::optional<std::string> & value
    );

    // 写入可选字段
    void write_optional_field(
        std::string_view name,
        const std::optional<std::size_t> & value
    );

    // 写入表达式子字段
    void write_child_field(
        std::string_view name,
        const ExpressionNode * expression
    );

    // 写入表达式子字段
    void write_child_field(
        std::string_view name,
        std::optional<const ExpressionNode &> expression
    );

    // 缩进作用域
    class IndentScope;

private:
    std::ostream & ostream_;
    AstDebugPrinterOptions options_;
    std::size_t indent_;
    std::string pending_str_; // 待处理的节点前缀
};

// 打印 AST 节点
std::string debug_print(
    const AstNode & node,
    AstDebugPrinterOptions options = {}
);

// 打印语句
std::string debug_print(
    const StatementNode & statement,
    AstDebugPrinterOptions options = {}
);

// 打印表达式
std::string debug_print(
    const ExpressionNode & expression,
    AstDebugPrinterOptions options = {}
);

// 打印 AST 节点
void debug_print(
    std::ostream & ostream,
    const AstNode & node,
    AstDebugPrinterOptions options = {}
);

// 打印语句
void debug_print(
    std::ostream & ostream,
    const StatementNode & statement,
    AstDebugPrinterOptions options = {}
);

// 打印表达式
void debug_print(
    std::ostream & ostream,
    const ExpressionNode & expression,
    AstDebugPrinterOptions options = {}
);

} // namespace litedb::core::parser::ast
