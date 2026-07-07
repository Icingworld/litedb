#pragma once

#include <memory>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::physical_plan
{

enum class PhysicalPlanNodeKind
{
    SeqScan,
    IndexScan,
    Filter,
    Projection,
    Sort,
    Limit,
};

class PhysicalPlanNode
{
public:
    PhysicalPlanNode(const PhysicalPlanNode &) = delete;

    PhysicalPlanNode & operator=(const PhysicalPlanNode &) = delete;

    PhysicalPlanNode(PhysicalPlanNode &&) noexcept = default;

    PhysicalPlanNode & operator=(PhysicalPlanNode &&) noexcept = default;

    virtual ~PhysicalPlanNode() noexcept = default;

protected:
    PhysicalPlanNode(PhysicalPlanNodeKind kind, parser::ast::AstNodeLocation location) noexcept;

public:
    [[nodiscard]]
    PhysicalPlanNodeKind kind() const noexcept;

    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

    [[nodiscard]]
    virtual std::unique_ptr<PhysicalPlanNode> clone() const = 0;

private:
    PhysicalPlanNodeKind kind_;
    parser::ast::AstNodeLocation location_;
};

} // namespace litedb::core::physical_plan
