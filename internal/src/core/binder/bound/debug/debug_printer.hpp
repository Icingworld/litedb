#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/binder/bound/dispatcher/expression_dispatcher.hpp"
#include "core/binder/bound/dispatcher/statement_dispatcher.hpp"

namespace litedb::core::binder::bound
{

struct BoundColumn;

// 绑定调试打印器选项
struct BoundDebugPrinterOptions
{
    bool include_type {true}; // 是否包含表达式和列的逻辑类型
};

// 绑定调试打印器
class BoundDebugPrinter final
    : private ConstBoundStatementDispatcher<BoundDebugPrinter, void>
    , private ConstBoundExpressionDispatcher<BoundDebugPrinter, void>
{
    friend ConstBoundStatementDispatcher<BoundDebugPrinter, void>;
    friend ConstBoundExpressionDispatcher<BoundDebugPrinter, void>;

public:
    explicit BoundDebugPrinter(std::ostream & ostream, BoundDebugPrinterOptions options = {});

public:
    // 打印绑定语句
    void print(const BoundStatement & statement);

    // 打印绑定表达式
    void print(const BoundExpression & expression);

private:
    // 访问 CREATE DATABASE 语句
    void visit_create_database_statement(const BoundCreateDatabaseStatement & statement);

    // 访问 CREATE COLLECTION 语句
    void visit_create_collection_statement(const BoundCreateCollectionStatement & statement);

    // 访问 CREATE INDEX 语句
    void visit_create_index_statement(const BoundCreateIndexStatement & statement);

    // 访问 CREATE VINDEX 语句
    void visit_create_vector_index_statement(const BoundCreateVectorIndexStatement & statement);

    // 访问 DELETE 语句
    void visit_delete_statement(const BoundDeleteStatement & statement);

    // 访问 DESCRIBE COLLECTION 语句
    void visit_describe_collection_statement(const BoundDescribeCollectionStatement & statement);

    // 访问 DROP DATABASE 语句
    void visit_drop_database_statement(const BoundDropDatabaseStatement & statement);

    // 访问 DROP COLLECTION 语句
    void visit_drop_collection_statement(const BoundDropCollectionStatement & statement);

    // 访问 DROP INDEX 语句
    void visit_drop_index_statement(const BoundDropIndexStatement & statement);

    // 访问 DROP VINDEX 语句
    void visit_drop_vector_index_statement(const BoundDropVectorIndexStatement & statement);

    // 访问 INSERT 语句
    void visit_insert_statement(const BoundInsertStatement & statement);

    // 访问 SELECT 语句
    void visit_select_statement(const BoundSelectStatement & statement);

    // 访问 SHOW DATABASES 语句
    void visit_show_databases_statement(const BoundShowDatabasesStatement & statement);

    // 访问 SHOW COLLECTIONS 语句
    void visit_show_collections_statement(const BoundShowCollectionsStatement & statement);

    // 访问 SHOW INDEXES 语句
    void visit_show_indexes_statement(const BoundShowIndexesStatement & statement);

    // 访问 SHOW VINDEXES 语句
    void visit_show_vector_indexes_statement(const BoundShowVectorIndexesStatement & statement);

    // 访问 UPDATE 语句
    void visit_update_statement(const BoundUpdateStatement & statement);

    // 访问 USE 语句
    void visit_use_statement(const BoundUseStatement & statement);

    // 访问字面量表达式
    void visit_literal_expression(const BoundLiteralExpression & expression);

    // 访问空值表达式
    void visit_null_expression(const BoundNullExpression & expression);

    // 访问列引用表达式
    void visit_column_ref_expression(const BoundColumnRefExpression & expression);

    // 访问一元表达式
    void visit_unary_expression(const BoundUnaryExpression & expression);

    // 访问二元表达式
    void visit_binary_expression(const BoundBinaryExpression & expression);

    // 访问向量表达式
    void visit_vector_expression(const BoundVectorExpression & expression);

    // 访问函数表达式
    void visit_function_expression(const BoundFunctionExpression & expression);

    // 访问 IN 表达式
    void visit_in_expression(const BoundInExpression & expression);

    // 访问 BETWEEN 表达式
    void visit_between_expression(const BoundBetweenExpression & expression);

    // 访问 LIKE 表达式
    void visit_like_expression(const BoundLikeExpression & expression);

    // 访问类型转换表达式
    void visit_cast_expression(const BoundCastExpression & expression);

    // 写入缩进
    void write_indent();

    // 写入节点头
    void write_node_header(std::string_view name);

    // 写入字段
    void write_field(std::string_view name, std::string_view value);

    // 写入字段
    void write_field(std::string_view name, bool value);

    // 写入字段
    void write_field(std::string_view name, std::size_t value);

    // 写入可选字段
    void write_optional_field(std::string_view name, const std::optional<std::string> & value);

    // 写入可选字段
    void write_optional_field(std::string_view name, const std::optional<std::size_t> & value);

    // 写入逻辑类型字段
    void write_type_field(std::string_view name, const common::LogicalType & type);

    // 写入绑定列
    void write_bound_column(const BoundColumn & column);

    // 写入表达式子字段
    void write_child_field(std::string_view name, const BoundExpression * expression);

    // 写入可选表达式子字段
    void
    write_child_field(std::string_view name, std::optional<const BoundExpression &> expression);

    // 缩进作用域
    class IndentScope;

private:
    std::ostream & ostream_;
    BoundDebugPrinterOptions options_;
    std::size_t indent_;
    std::string pending_str_;
};

// 打印绑定语句
std::string debug_print(const BoundStatement & statement, BoundDebugPrinterOptions options = {});

// 打印绑定表达式
std::string debug_print(const BoundExpression & expression, BoundDebugPrinterOptions options = {});

// 将绑定语句打印到输出流
void debug_print(
    std::ostream & ostream,
    const BoundStatement & statement,
    BoundDebugPrinterOptions options = {}
);

// 将绑定表达式打印到输出流
void debug_print(
    std::ostream & ostream,
    const BoundExpression & expression,
    BoundDebugPrinterOptions options = {}
);

} // namespace litedb::core::binder::bound
