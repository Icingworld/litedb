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

/**
 * @brief 绑定调试打印器选项
 */
struct BoundDebugPrinterOptions
{
    bool include_type {true};    ///< 是否包含表达式和列的逻辑类型
};

/**
 * @brief 绑定调试打印器
 */
class BoundDebugPrinter final
    : private BoundStatementDispatcher<BoundDebugPrinter>
    , private BoundExpressionDispatcher<BoundDebugPrinter>
{
    friend class BoundStatementDispatcher<BoundDebugPrinter>;
    friend class BoundExpressionDispatcher<BoundDebugPrinter>;

public:
    explicit BoundDebugPrinter(
        std::ostream & ostream,
        BoundDebugPrinterOptions options = {}
    );

public:
    /**
     * @brief 打印绑定语句
     * @param statement 绑定语句
     */
    void print(const BoundStatement & statement);

    /**
     * @brief 打印绑定表达式
     * @param expression 绑定表达式
     */
    void print(const BoundExpression & expression);

private:
    /**
     * @brief 访问 CREATE DATABASE 语句
     * @param statement CREATE DATABASE 语句
     */
    void visit_create_database_statement(
        const BoundCreateDatabaseStatement & statement
    );

    /**
     * @brief 访问 CREATE COLLECTION 语句
     * @param statement CREATE COLLECTION 语句
     */
    void visit_create_collection_statement(
        const BoundCreateCollectionStatement & statement
    );

    /**
     * @brief 访问 CREATE INDEX 语句
     * @param statement CREATE INDEX 语句
     */
    void visit_create_index_statement(
        const BoundCreateIndexStatement & statement
    );

    /**
     * @brief 访问 CREATE VINDEX 语句
     * @param statement CREATE VINDEX 语句
     */
    void visit_create_vector_index_statement(
        const BoundCreateVectorIndexStatement & statement
    );

    /**
     * @brief 访问 DELETE 语句
     * @param statement DELETE 语句
     */
    void visit_delete_statement(
        const BoundDeleteStatement & statement
    );

    /**
     * @brief 访问 DESCRIBE COLLECTION 语句
     * @param statement DESCRIBE COLLECTION 语句
     */
    void visit_describe_collection_statement(
        const BoundDescribeCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP DATABASE 语句
     * @param statement DROP DATABASE 语句
     */
    void visit_drop_database_statement(
        const BoundDropDatabaseStatement & statement
    );

    /**
     * @brief 访问 DROP COLLECTION 语句
     * @param statement DROP COLLECTION 语句
     */
    void visit_drop_collection_statement(
        const BoundDropCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP INDEX 语句
     * @param statement DROP INDEX 语句
     */
    void visit_drop_index_statement(
        const BoundDropIndexStatement & statement
    );

    /**
     * @brief 访问 DROP VINDEX 语句
     * @param statement DROP VINDEX 语句
     */
    void visit_drop_vector_index_statement(
        const BoundDropVectorIndexStatement & statement
    );

    /**
     * @brief 访问 INSERT 语句
     * @param statement INSERT 语句
     */
    void visit_insert_statement(
        const BoundInsertStatement & statement
    );

    /**
     * @brief 访问 SELECT 语句
     * @param statement SELECT 语句
     */
    void visit_select_statement(
        const BoundSelectStatement & statement
    );

    /**
     * @brief 访问 SHOW DATABASES 语句
     * @param statement SHOW DATABASES 语句
     */
    void visit_show_databases_statement(
        const BoundShowDatabasesStatement & statement
    );

    /**
     * @brief 访问 SHOW COLLECTIONS 语句
     * @param statement SHOW COLLECTIONS 语句
     */
    void visit_show_collections_statement(
        const BoundShowCollectionsStatement & statement
    );

    /**
     * @brief 访问 SHOW INDEXES 语句
     * @param statement SHOW INDEXES 语句
     */
    void visit_show_indexes_statement(
        const BoundShowIndexesStatement & statement
    );

    /**
     * @brief 访问 SHOW VINDEXES 语句
     * @param statement SHOW VINDEXES 语句
     */
    void visit_show_vector_indexes_statement(
        const BoundShowVectorIndexesStatement & statement
    );

    /**
     * @brief 访问 UPDATE 语句
     * @param statement UPDATE 语句
     */
    void visit_update_statement(
        const BoundUpdateStatement & statement
    );

    /**
     * @brief 访问 USE 语句
     * @param statement USE 语句
     */
    void visit_use_statement(
        const BoundUseStatement & statement
    );

    /**
     * @brief 访问字面量表达式
     * @param expression 字面量表达式
     */
    void visit_literal_expression(
        const BoundLiteralExpression & expression
    );

    /**
     * @brief 访问空值表达式
     * @param expression 空值表达式
     */
    void visit_null_expression(
        const BoundNullExpression & expression
    );

    /**
     * @brief 访问列引用表达式
     * @param expression 列引用表达式
     */
    void visit_column_ref_expression(
        const BoundColumnRefExpression & expression
    );

    /**
     * @brief 访问一元表达式
     * @param expression 一元表达式
     */
    void visit_unary_expression(
        const BoundUnaryExpression & expression
    );

    /**
     * @brief 访问二元表达式
     * @param expression 二元表达式
     */
    void visit_binary_expression(
        const BoundBinaryExpression & expression
    );

    /**
     * @brief 访问向量表达式
     * @param expression 向量表达式
     */
    void visit_vector_expression(
        const BoundVectorExpression & expression
    );

    /**
     * @brief 访问函数表达式
     * @param expression 函数表达式
     */
    void visit_function_expression(
        const BoundFunctionExpression & expression
    );

    /**
     * @brief 访问 IN 表达式
     * @param expression IN 表达式
     */
    void visit_in_expression(
        const BoundInExpression & expression
    );

    /**
     * @brief 访问 BETWEEN 表达式
     * @param expression BETWEEN 表达式
     */
    void visit_between_expression(
        const BoundBetweenExpression & expression
    );

    /**
     * @brief 访问 LIKE 表达式
     * @param expression LIKE 表达式
     */
    void visit_like_expression(
        const BoundLikeExpression & expression
    );

    /**
     * @brief 访问类型转换表达式
     * @param expression 类型转换表达式
     */
    void visit_cast_expression(
        const BoundCastExpression & expression
    );

    /**
     * @brief 写入缩进
     */
    void write_indent();

    /**
     * @brief 写入节点头
     * @param name 节点名称
     */
    void write_node_header(std::string_view name);

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
     * @brief 写入逻辑类型字段
     * @param name 字段名称
     * @param type 逻辑类型
     */
    void write_type_field(
        std::string_view name,
        const common::LogicalType & type
    );

    /**
     * @brief 写入绑定列
     * @param column 绑定列
     */
    void write_bound_column(const BoundColumn & column);

    /**
     * @brief 写入表达式子字段
     * @param name 字段名称
     * @param expression 表达式，允许为空
     */
    void write_child_field(
        std::string_view name,
        const BoundExpression * expression
    );

    /**
     * @brief 缩进作用域
     */
    class IndentScope;

private:
    std::ostream & ostream_;              ///< 输出流
    BoundDebugPrinterOptions options_;    ///< 打印选项
    std::size_t indent_;                  ///< 缩进
    std::string pending_str_;             ///< 待处理的节点前缀
};

/**
 * @brief 打印绑定语句
 * @param statement 绑定语句
 * @return 打印结果
 */
std::string debug_print(
    const BoundStatement & statement,
    BoundDebugPrinterOptions options = {}
);

/**
 * @brief 打印绑定表达式
 * @param expression 绑定表达式
 * @return 打印结果
 */
std::string debug_print(
    const BoundExpression & expression,
    BoundDebugPrinterOptions options = {}
);

/**
 * @brief 将绑定语句打印到输出流
 * @param ostream 输出流
 * @param statement 绑定语句
 */
void debug_print(
    std::ostream & ostream,
    const BoundStatement & statement,
    BoundDebugPrinterOptions options = {}
);

/**
 * @brief 将绑定表达式打印到输出流
 * @param ostream 输出流
 * @param expression 绑定表达式
 */
void debug_print(
    std::ostream & ostream,
    const BoundExpression & expression,
    BoundDebugPrinterOptions options = {}
);

} // namespace litedb::core::binder::bound
