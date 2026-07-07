#pragma once

namespace litedb::core::planner::logical
{

class LogicalScan;
class LogicalFilter;
class LogicalProjection;
class LogicalOrderBy;
class LogicalLimit;

/**
 * @brief 逻辑计划节点访问器
 */
class LogicalPlanNodeVisitor
{
public:
    virtual ~LogicalPlanNodeVisitor() noexcept = default;

public:
    virtual void visit(const LogicalScan & node) = 0;
    virtual void visit(const LogicalFilter & node) = 0;
    virtual void visit(const LogicalProjection & node) = 0;
    virtual void visit(const LogicalOrderBy & node) = 0;
    virtual void visit(const LogicalLimit & node) = 0;
};

} // namespace litedb::core::planner::logical
