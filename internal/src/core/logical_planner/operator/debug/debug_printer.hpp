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

// 逻辑算子调试打印器
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
    // 打印逻辑算子
    void print(const LogicalPlanOperator & op);

private:
    // 访问扫描算子
    void visit_scan_operator(const LogicalScanOperator & op);

    // 访问过滤算子
    void visit_filter_operator(const LogicalFilterOperator & op);

    // 访问投影算子
    void visit_projection_operator(const LogicalProjectionOperator & op);

    // 访问排序算子
    void visit_order_by_operator(const LogicalOrderByOperator & op);

    // 访问限制算子
    void visit_limit_operator(const LogicalLimitOperator & op);

private:
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
    void write_optional_field(
        std::string_view name,
        const std::optional<std::string> & value
    );

    // 写入可选字段
    void write_optional_field(
        std::string_view name,
        const std::optional<std::size_t> & value
    );

    // 写入逻辑类型字段
    void write_type_field(
        std::string_view name,
        const common::LogicalType & type
    );

    // 写入绑定表达式字段
    void write_expression_field(
        std::string_view name,
        const binder::bound::BoundExpression & expression
    );

    // 写入子算子字段
    void write_child_field(
        std::string_view name,
        const LogicalPlanOperator * child
    );

    // 缩进作用域
    class IndentScope;

private:
    std::ostream & ostream_;
    std::size_t indent_;
    std::string pending_str_;
};

// 调试打印逻辑算子
[[nodiscard]]
std::string debug_print(const LogicalPlanOperator & op);

// 调试打印逻辑算子
void debug_print(
    std::ostream & ostream,
    const LogicalPlanOperator & op
);

} // namespace litedb::core::logical_planner::op
