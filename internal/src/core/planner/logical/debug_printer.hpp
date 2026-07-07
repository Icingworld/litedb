#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"
#include "core/planner/logical/node/logical_plan_node_visitor.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑计划节点调试打印选项
 */
struct LogicalDebugPrinterOptions
{
    bool include_location {true};               ///< 是否包含位置信息
    bool include_expression_type {true};        ///< 是否包含表达式类型
};

/**
 * @brief 逻辑计划节点调试打印器
 */
class LogicalDebugPrinter final : public LogicalPlanNodeVisitor
{
public:
    explicit LogicalDebugPrinter(std::ostream & out, LogicalDebugPrinterOptions options = {});

public:
    /**
     * @brief 打印逻辑计划节点
     * @param node 逻辑计划节点
     */
    void print(const LogicalPlanNode & node);

public:
    void visit(const LogicalScan & node) override;
    void visit(const LogicalFilter & node) override;
    void visit(const LogicalIndexScan & node) override;
    void visit(const LogicalProjection & node) override;
    void visit(const LogicalOrderBy & node) override;
    void visit(const LogicalLimit & node) override;

private:
    /**
     * @brief 缩进范围
     */
    class IndentScope;

    /**
     * @brief 写入缩进
     */
    void write_indent();

    /**
     * @brief 写入节点头
     * @param name 节点名称
     * @param location 位置
     */
    void write_node_header(const char * name, parser::ast::AstNodeLocation location);

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
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_optional_field(const char * name, std::optional<std::size_t> value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_bound_expression_field(const char * name, const binder::bound::BoundExpression & expression);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_child_field(const char * name, const LogicalPlanNode & child);

private:
    std::ostream & out_;                        ///< 输出流
    LogicalDebugPrinterOptions options_;        ///< 打印选项
    std::size_t indent_ {0};                    ///< 缩进
};

/**
 * @brief 调试打印逻辑计划节点
 * @param node 逻辑计划节点
 * @param options 打印选项
 * @return 调试打印结果
 */
std::string debug_print(const LogicalPlanNode & node, LogicalDebugPrinterOptions options = {});

/**
 * @brief 调试打印逻辑计划节点
 * @param out 输出流
 * @param node 逻辑计划节点
 * @param options 打印选项
 */
void debug_print(std::ostream & out, const LogicalPlanNode & node, LogicalDebugPrinterOptions options = {});

} // namespace litedb::core::planner::logical
