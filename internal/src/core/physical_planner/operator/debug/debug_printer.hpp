#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/physical_planner/operator/dispatcher/physical_operator_dispatcher.hpp"

namespace litedb::core::physical_planner::op
{

/**
 * @brief 物理算子调试打印器
 */
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
    /**
     * @brief 打印物理算子
     * @param op 物理算子
     */
    void print(const PhysicalOperator & op);

private:
    /**
     * @brief 访问顺序扫描算子
     * @param op 顺序扫描算子
     */
    void visit_seq_scan_operator(const SeqScanOperator & op);

    /**
     * @brief 访问索引扫描算子
     * @param op 索引扫描算子
     */
    void visit_index_scan_operator(const IndexScanOperator & op);

    /**
     * @brief 访问向量检索算子
     * @param op 向量检索算子
     */
    void visit_vector_search_operator(const VectorSearchOperator & op);

    /**
     * @brief 访问过滤算子
     * @param op 过滤算子
     */
    void visit_filter_operator(const FilterOperator & op);

    /**
     * @brief 访问投影算子
     * @param op 投影算子
     */
    void visit_projection_operator(const ProjectionOperator & op);

    /**
     * @brief 访问排序算子
     * @param op 排序算子
     */
    void visit_sort_operator(const SortOperator & op);

    /**
     * @brief 访问限制算子
     * @param op 限制算子
     */
    void visit_limit_operator(const LimitOperator & op);

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
        const PhysicalOperator * child
    );

    /**
     * @brief 缩进作用域
     */
    class IndentScope;

private:
    std::ostream & ostream_;              ///< 输出流
    std::size_t indent_;                  ///< 缩进
    std::string pending_str_;             ///< 待处理的节点前缀
};

/**
 * @brief 调试打印物理算子
 * @param op 物理算子
 * @return 调试打印结果
 */
[[nodiscard]]
std::string debug_print(const PhysicalOperator & op);

/**
 * @brief 调试打印物理算子
 * @param ostream 输出流
 * @param op 物理算子
 */
void debug_print(
    std::ostream & ostream,
    const PhysicalOperator & op
);

} // namespace litedb::core::physical_planner::op
