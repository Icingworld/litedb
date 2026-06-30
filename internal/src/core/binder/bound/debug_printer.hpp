#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/expression/bound_expression_visitor.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/bound/statement/bound_statement_visitor.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定调试打印器选项
 */
struct BoundDebugPrinterOptions
{
    bool include_location {true};  ///< 是否包含位置信息
    bool include_type {true};      ///< 是否包含类型信息
};

/**
 * @brief 绑定调试打印器
 */
class BoundDebugPrinter final : public BoundStatementVisitor, public BoundExpressionVisitor
{
public:
    explicit BoundDebugPrinter(std::ostream & out, BoundDebugPrinterOptions options = {});

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

public:
    void visit(const BoundCreateDatabaseStatement & statement) override;
    void visit(const BoundCreateCollectionStatement & statement) override;
    void visit(const BoundCreateIndexStatement & statement) override;
    void visit(const BoundCreateVectorIndexStatement & statement) override;
    void visit(const BoundDeleteStatement & statement) override;
    void visit(const BoundDescribeCollectionStatement & statement) override;
    void visit(const BoundDropDatabaseStatement & statement) override;
    void visit(const BoundDropCollectionStatement & statement) override;
    void visit(const BoundDropIndexStatement & statement) override;
    void visit(const BoundDropVectorIndexStatement & statement) override;
    void visit(const BoundInsertStatement & statement) override;
    void visit(const BoundSelectStatement & statement) override;
    void visit(const BoundShowDatabasesStatement & statement) override;
    void visit(const BoundShowCollectionsStatement & statement) override;
    void visit(const BoundShowIndexesStatement & statement) override;
    void visit(const BoundShowVectorIndexesStatement & statement) override;
    void visit(const BoundUpdateStatement & statement) override;
    void visit(const BoundUseStatement & statement) override;

    void visit(const BoundBetweenExpression & expression) override;
    void visit(const BoundBinaryExpression & expression) override;
    void visit(const BoundCastExpression & expression) override;
    void visit(const BoundColumnRefExpression & expression) override;
    void visit(const BoundFunctionExpression & expression) override;
    void visit(const BoundInExpression & expression) override;
    void visit(const BoundLikeExpression & expression) override;
    void visit(const BoundLiteralExpression & expression) override;
    void visit(const BoundNullExpression & expression) override;
    void visit(const BoundUnaryExpression & expression) override;
    void visit(const BoundVectorExpression & expression) override;
    void visit(const BoundWildcardExpression & expression) override;

private:
    /**
     * @brief 缩进作用域
     */
    class IndentScope;

    /**
     * @brief 写入缩进
     */
    void write_indent();

    /**
     * @brief 写入节点头
     */
    void write_node_header(const char * name, parser::ast::AstNodeLocation location);

    /**
     * @brief 写入字段
     */
    void write_field(const char * name, const std::string & value);

    /**
     * @brief 写入字段
     */
    void write_field(const char * name, const char * value);

    /**
     * @brief 写入字段
     */
    void write_field(const char * name, bool value);

    /**
     * @brief 写入字段
     */
    void write_field(const char * name, std::size_t value);

    /**
     * @brief 写入字段
     */
    void write_optional_field(const char * name, std::optional<std::size_t> value);

    /**
     * @brief 写入字段
     */
    void write_type_field(const char * name, const common::LogicalType & type);

    /**
     * @brief 写入表达式头
     */
    void write_expression_header(const char * name, const BoundExpression & expression);

    /**
     * @brief 写入子字段
     */
    void write_child_field(const char * name, const BoundExpression * expression);

    /**
     * @brief 写入绑定列
     */
    void write_bound_column(const BoundColumn & column);

private:
    std::ostream & out_;                  ///< 输出流
    BoundDebugPrinterOptions options_;    ///< 选项
    std::size_t indent_ {0};              ///< 缩进
    std::string pending_prefix_;          ///< 待处理前缀
};

/**
 * @brief 绑定调试打印
 * @param statement 绑定语句
 * @param options 选项
 * @return 绑定调试打印结果
 */
std::string debug_print(const BoundStatement & statement, BoundDebugPrinterOptions options = {});

/**
 * @brief 绑定调试打印
 * @param expression 绑定表达式
 * @param options 选项
 * @return 绑定调试打印结果
 */
std::string debug_print(const BoundExpression & expression, BoundDebugPrinterOptions options = {});

/**
 * @brief 绑定调试打印
 * @param out 输出流
 * @param statement 绑定语句
 * @param options 选项
 */
void debug_print(std::ostream & out, const BoundStatement & statement, BoundDebugPrinterOptions options = {});

/**
 * @brief 绑定调试打印
 * @param out 输出流
 * @param expression 绑定表达式
 * @param options 选项
 */
void debug_print(std::ostream & out, const BoundExpression & expression, BoundDebugPrinterOptions options = {});

} // namespace litedb::core::binder::bound
