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

/**
 * @brief AST 调试打印器选项
 */
struct AstDebugPrinterOptions
{
    bool include_location {true};    ///< 是否包含位置信息
};

/**
 * @brief AST 调试打印器
 */
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
    /**
     * @brief 打印任意 AST 节点
     * @param node AST 节点
     */
    void print(const AstNode & node);

    /**
     * @brief 打印语句
     * @param statement 语句
     */
    void print(const StatementNode & statement);

    /**
     * @brief 打印表达式
     * @param expression 表达式
     */
    void print(const ExpressionNode & expression);

private:
    /**
     * @brief 访问 CREATE DATABASE 语句
     * @param statement CREATE DATABASE 语句
     */
    void visit_create_database_statement(
        const CreateDatabaseStatement & statement
    );

    /**
     * @brief 访问 CREATE COLLECTION 语句
     * @param statement CREATE COLLECTION 语句
     */
    void visit_create_collection_statement(
        const CreateCollectionStatement & statement
    );

    /**
     * @brief 访问 CREATE INDEX 语句
     * @param statement CREATE INDEX 语句
     */
    void visit_create_index_statement(
        const CreateIndexStatement & statement
    );

    /**
     * @brief 访问 CREATE VINDEX 语句
     * @param statement CREATE VINDEX 语句
     */
    void visit_create_vector_index_statement(
        const CreateVectorIndexStatement & statement
    );

    /**
     * @brief 访问 DELETE 语句
     * @param statement DELETE 语句
     */
    void visit_delete_statement(
        const DeleteStatement & statement
    );

    /**
     * @brief 访问 DESCRIBE COLLECTION 语句
     * @param statement DESCRIBE COLLECTION 语句
     */
    void visit_describe_collection_statement(
        const DescribeCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP DATABASE 语句
     * @param statement DROP DATABASE 语句
     */
    void visit_drop_database_statement(
        const DropDatabaseStatement & statement
    );

    /**
     * @brief 访问 DROP COLLECTION 语句
     * @param statement DROP COLLECTION 语句
     */
    void visit_drop_collection_statement(
        const DropCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP INDEX 语句
     * @param statement DROP INDEX 语句
     */
    void visit_drop_index_statement(
        const DropIndexStatement & statement
    );

    /**
     * @brief 访问 DROP VINDEX 语句
     * @param statement DROP VINDEX 语句
     */
    void visit_drop_vector_index_statement(
        const DropVectorIndexStatement & statement
    );

    /**
     * @brief 访问 INSERT 语句
     * @param statement INSERT 语句
     */
    void visit_insert_statement(
        const InsertStatement & statement
    );

    /**
     * @brief 访问 SELECT 语句
     * @param statement SELECT 语句
     */
    void visit_select_statement(
        const SelectStatement & statement
    );

    /**
     * @brief 访问 SHOW DATABASES 语句
     * @param statement SHOW DATABASES 语句
     */
    void visit_show_databases_statement(
        const ShowDatabasesStatement & statement
    );

    /**
     * @brief 访问 SHOW COLLECTIONS 语句
     * @param statement SHOW COLLECTIONS 语句
     */
    void visit_show_collections_statement(
        const ShowCollectionsStatement & statement
    );

    /**
     * @brief 访问 SHOW INDEXES 语句
     * @param statement SHOW INDEXES 语句
     */
    void visit_show_indexes_statement(
        const ShowIndexesStatement & statement
    );

    /**
     * @brief 访问 SHOW VINDEXES 语句
     * @param statement SHOW VINDEXES 语句
     */
    void visit_show_vector_indexes_statement(
        const ShowVectorIndexesStatement & statement
    );

    /**
     * @brief 访问 UPDATE 语句
     * @param statement UPDATE 语句
     */
    void visit_update_statement(
        const UpdateStatement & statement
    );

    /**
     * @brief 访问 USE 语句
     * @param statement USE 语句
     */
    void visit_use_statement(
        const UseStatement & statement
    );

    /**
     * @brief 访问标识符表达式
     * @param expression 标识符表达式
     */
    void visit_identifier_expression(
        const IdentifierExpression & expression
    );

    /**
     * @brief 访问通配符表达式
     * @param expression 通配符表达式
     */
    void visit_wildcard_expression(
        const WildcardExpression & expression
    );

    /**
     * @brief 访问字面量表达式
     * @param expression 字面量表达式
     */
    void visit_literal_expression(
        const LiteralExpression & expression
    );

    /**
     * @brief 访问函数调用表达式
     * @param expression 函数调用表达式
     */
    void visit_function_call_expression(
        const FunctionCallExpression & expression
    );

    /**
     * @brief 访问列引用表达式
     * @param expression 列引用表达式
     */
    void visit_column_reference_expression(
        const ColumnReferenceExpression & expression
    );

    /**
     * @brief 访问向量表达式
     * @param expression 向量表达式
     */
    void visit_vector_expression(
        const VectorExpression & expression
    );

    /**
     * @brief 访问二元表达式
     * @param expression 二元表达式
     */
    void visit_binary_expression(
        const BinaryExpression & expression
    );

    /**
     * @brief 访问一元表达式
     * @param expression 一元表达式
     */
    void visit_unary_expression(
        const UnaryExpression & expression
    );

    /**
     * @brief 访问 IN 表达式
     * @param expression IN 表达式
     */
    void visit_in_expression(
        const InExpression & expression
    );

    /**
     * @brief 访问 BETWEEN 表达式
     * @param expression BETWEEN 表达式
     */
    void visit_between_expression(
        const BetweenExpression & expression
    );

    /**
     * @brief 访问 LIKE 表达式
     * @param expression LIKE 表达式
     */
    void visit_like_expression(
        const LikeExpression & expression
    );

    /**
     * @brief 访问别名表达式
     * @param expression 别名表达式
     */
    void visit_alias_expression(
        const AliasExpression & expression
    );

    /**
     * @brief 写入缩进
     */
    void write_indent();

    /**
     * @brief 写入节点头
     * @param name 节点名称
     * @param location 节点位置
     */
    void write_node_header(std::string_view name, AstNodeLocation location);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(std::string_view name, std::string_view value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(std::string_view name, bool value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(std::string_view name, std::size_t value);

    /**
     * @brief 写入可选字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_optional_field(
        std::string_view name,
        const std::optional<std::string> & value
    );

    /**
     * @brief 写入可选字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_optional_field(
        std::string_view name,
        const std::optional<std::size_t> & value
    );

    /**
     * @brief 写入表达式子字段
     * @param name 字段名称
     * @param expression 表达式，允许为空
     */
    void write_child_field(
        std::string_view name,
        const ExpressionNode * expression
    );

    /**
     * @brief 缩进作用域
     */
    class IndentScope;

private:
    std::ostream & ostream_;              ///< 输出流
    AstDebugPrinterOptions options_;      ///< 打印选项
    std::size_t indent_;                  ///< 缩进
    std::string pending_str_;             ///< 待处理的节点前缀
};

/**
 * @brief 打印 AST 节点
 * @param node AST 节点
 * @param options 打印选项
 * @return 打印结果
 */
std::string debug_print(
    const AstNode & node,
    AstDebugPrinterOptions options = {}
);

/**
 * @brief 打印语句
 * @param statement 语句
 * @param options 打印选项
 * @return 打印结果
 */
std::string debug_print(
    const StatementNode & statement,
    AstDebugPrinterOptions options = {}
);

/**
 * @brief 打印表达式
 * @param expression 表达式
 * @param options 打印选项
 * @return 打印结果
 */
std::string debug_print(
    const ExpressionNode & expression,
    AstDebugPrinterOptions options = {}
);

/**
 * @brief 打印 AST 节点
 * @param ostream 输出流
 * @param node AST 节点
 * @param options 打印选项
 */
void debug_print(
    std::ostream & ostream,
    const AstNode & node,
    AstDebugPrinterOptions options = {}
);

/**
 * @brief 打印语句
 * @param ostream 输出流
 * @param statement 语句
 * @param options 打印选项
 */
void debug_print(
    std::ostream & ostream,
    const StatementNode & statement,
    AstDebugPrinterOptions options = {}
);

/**
 * @brief 打印表达式
 * @param ostream 输出流
 * @param expression 表达式
 * @param options 打印选项
 */
void debug_print(
    std::ostream & ostream,
    const ExpressionNode & expression,
    AstDebugPrinterOptions options = {}
);

} // namespace litedb::core::parser::ast
