#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/common/logical_type.hpp"
#include "core/logical_planner/operator/dispatcher/logical_operator_dispatcher.hpp"

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑算子调试打印器
 */
class LogicalPlanOperatorDebugPrinter
    : private ConstLogicalOperatorDispatcher<
          LogicalPlanOperatorDebugPrinter,
          void
      >
{
    friend ConstLogicalOperatorDispatcher<
        LogicalPlanOperatorDebugPrinter,
        void
    >;

public:
    explicit LogicalPlanOperatorDebugPrinter(std::ostream & ostream);

public:
    /**
     * @brief 打印逻辑算子
     * @param op 逻辑算子
     */
    void print(const LogicalPlanOperator & op);

private:
    /**
     * @brief 访问扫描算子
     * @param op 扫描算子
     */
    void visit_scan_operator(const LogicalScanOperator & op);

    /**
     * @brief 访问过滤算子
     * @param op 过滤算子
     */
    void visit_filter_operator(const LogicalFilterOperator & op);

    /**
     * @brief 访问投影算子
     * @param op 投影算子
     */
    void visit_projection_operator(const LogicalProjectionOperator & op);

    /**
     * @brief 访问排序算子
     * @param op 排序算子
     */
    void visit_order_by_operator(const LogicalOrderByOperator & op);

    /**
     * @brief 访问限制算子
     * @param op 限制算子
     */
    void visit_limit_operator(const LogicalLimitOperator & op);

private:
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
     * @brief 写入绑定表达式字段
     * @param name 字段名称
     * @param expression 绑定表达式
     */
    void write_expression_field(
        std::string_view name,
        const binder::bound::BoundExpression & expression
    );

    /**
     * @brief 写入子算子字段
     * @param name 字段名称
     * @param child 子算子，允许为空
     */
    void write_child_field(
        std::string_view name,
        const LogicalPlanOperator * child
    );

    /**
     * @brief 缩进作用域
     */
    class IndentScope;

private:
    std::ostream & ostream_;              // 输出流
    std::size_t indent_;                  // 缩进
    std::string pending_str_;             // 待处理的节点前缀
};

/**
 * @brief 调试打印逻辑算子
 * @param op 逻辑算子
 * @return 调试打印结果
 */
[[nodiscard]]
std::string debug_print(const LogicalPlanOperator & op);

/**
 * @brief 调试打印逻辑算子
 * @param ostream 输出流
 * @param op 逻辑算子
 */
void debug_print(
    std::ostream & ostream,
    const LogicalPlanOperator & op
);

} // namespace litedb::core::logical_planner::op
