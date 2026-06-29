#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 调试打印器选项
 */
struct DebugPrinterOptions
{
    bool include_location {true};  ///< 是否包含位置信息
};

/**
 * @brief 调试打印器
 */
class AstDebugPrinter final : public AstNodeVisitor
{
public:
    explicit AstDebugPrinter(std::ostream & out, DebugPrinterOptions options = {});

public:
    /**
     * @brief 打印节点
     * @param node 节点
     */
    void print(const AstNode & node);

public:
    void visit(const AlterStatement & node) override;
    void visit(const CreateCollectionStatement & node) override;
    void visit(const CreateDatabaseStatement & node) override;
    void visit(const CreateIndexStatement & node) override;
    void visit(const CreateVectorIndexStatement & node) override;
    void visit(const DeleteStatement & node) override;
    void visit(const DescribeStatement & node) override;
    void visit(const DropCollectionStatement & node) override;
    void visit(const DropDatabaseStatement & node) override;
    void visit(const DropIndexStatement & node) override;
    void visit(const DropVectorIndexStatement & node) override;
    void visit(const InsertStatement & node) override;
    void visit(const SelectStatement & node) override;
    void visit(const ShowStatement & node) override;
    void visit(const ShowCollectionsStatement & node) override;
    void visit(const ShowDatabasesStatement & node) override;
    void visit(const ShowIndexesStatement & node) override;
    void visit(const ShowVectorIndexesStatement & node) override;
    void visit(const UpdateStatement & node) override;
    void visit(const UseStatement & node) override;

    void visit(const AliasExpression & node) override;
    void visit(const BetweenExpression & node) override;
    void visit(const BinaryExpression & node) override;
    void visit(const ColumnReferenceExpression & node) override;
    void visit(const FunctionCallExpression & node) override;
    void visit(const IdentifierExpression & node) override;
    void visit(const InExpression & node) override;
    void visit(const LikeExpression & node) override;
    void visit(const LiteralExpression & node) override;
    void visit(const UnaryExpression & node) override;
    void visit(const VectorExpression & node) override;
    void visit(const WildcardExpression & node) override;

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
     * @param name 节点名称
     * @param location 节点位置
     */
    void write_node_header(const char * name, AstNodeLocation location);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(const char * name, const std::string & value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(const char * name, const char * value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(const char * name, bool value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(const char * name, std::size_t value);

    /**
     * @brief 写入可选字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_optional_field(const char * name, const std::optional<std::size_t> & value);

    /**
     * @brief 写入子字段
     * @param name 字段名称
     * @param node 子节点
     */
    void write_child_field(const char * name, const AstNode * node);

private:
    std::ostream & out_;                ///< 输出流
    DebugPrinterOptions options_;       ///< 选项
    std::size_t indent_ {0};            ///< 缩进
    std::string pending_prefix_;        ///< 待处理前缀
};

/**
 * @brief 调试打印
 * @param node 节点
 * @param options 选项
 * @return 调试打印结果
 */
std::string debug_print(const AstNode & node, DebugPrinterOptions options = {});

/**
 * @brief 调试打印
 * @param out 输出流
 * @param node 节点
 * @param options 选项
 */
void debug_print(std::ostream & out, const AstNode & node, DebugPrinterOptions options = {});

} // namespace litedb::core::parser::ast
