#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/physical_planner/operator/dispatcher/physical_operator_dispatcher.hpp"

namespace litedb::core::physical_planner::op
{

// 物理算子调试打印器
class PhysicalOperatorDebugPrinter
    : private ConstPhysicalOperatorDispatcher<
          PhysicalOperatorDebugPrinter,
          void
      >
{
    friend ConstPhysicalOperatorDispatcher<
        PhysicalOperatorDebugPrinter,
        void
    >;

public:
    explicit PhysicalOperatorDebugPrinter(std::ostream & ostream);

public:
    // 打印物理算子
    void print(const PhysicalOperator & op);

private:
    // 访问顺序扫描算子
    void visit_seq_scan_operator(const SeqScanOperator & op);

    // 访问索引扫描算子
    void visit_index_scan_operator(const IndexScanOperator & op);

    // 访问向量检索算子
    void visit_vector_search_operator(const VectorSearchOperator & op);

    // 访问过滤算子
    void visit_filter_operator(const FilterOperator & op);

    // 访问投影算子
    void visit_projection_operator(const ProjectionOperator & op);

    // 访问排序算子
    void visit_sort_operator(const SortOperator & op);

    // 访问限制算子
    void visit_limit_operator(const LimitOperator & op);

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

    // 写入绑定表达式字段
    void write_expression_field(
        std::string_view name,
        const binder::bound::BoundExpression & expression
    );

    // 写入可选绑定表达式字段
    void write_expression_field(
        std::string_view name,
        std::optional<const binder::bound::BoundExpression &> expression
    );

    // 写入子算子字段
    void write_child_field(
        std::string_view name,
        const PhysicalOperator * child
    );

    // 缩进作用域
    class IndentScope;

private:
    std::ostream & ostream_;
    std::size_t indent_;
    std::string pending_str_;
};

// 调试打印物理算子
[[nodiscard]]
std::string debug_print(const PhysicalOperator & op);

// 调试打印物理算子
void debug_print(
    std::ostream & ostream,
    const PhysicalOperator & op
);

} // namespace litedb::core::physical_planner::op
