#pragma once

#include <cstddef>
#include <iosfwd>
#include <string_view>

#include "core/physical_planner/operator/dispatcher/physical_operator_dispatcher.hpp"

namespace litedb::core::physical_planner::op
{

class PhysicalOperatorDebugPrinter
    : private ConstPhysicalOperatorDispatcher<PhysicalOperatorDebugPrinter, void>
{
    friend ConstPhysicalOperatorDispatcher<PhysicalOperatorDebugPrinter, void>;

public:
    explicit PhysicalOperatorDebugPrinter(std::ostream & ostream);

    void print(const PhysicalOperator & op);

private:
    void visit_seq_scan_operator(const SeqScanOperator & op);
    void visit_index_scan_operator(const IndexScanOperator & op);
    void visit_vector_search_operator(const VectorSearchOperator & op);
    void visit_filter_operator(const FilterOperator & op);
    void visit_projection_operator(const ProjectionOperator & op);
    void visit_sort_operator(const SortOperator & op);
    void visit_limit_operator(const LimitOperator & op);

    void indent();
    void header(std::string_view name);
    void field(std::string_view name, std::string_view value);
    void field(std::string_view name, std::size_t value);
    void expression(std::string_view name, const binder::bound::BoundExpression & value);
    void child(const PhysicalOperator & value);

    std::ostream & ostream_;
    std::size_t indent_ {0};
};

[[nodiscard]] std::string debug_print(const PhysicalOperator & op);
void debug_print(std::ostream & ostream, const PhysicalOperator & op);

} // namespace litedb::core::physical_planner::op
