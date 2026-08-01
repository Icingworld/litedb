#pragma once

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::physical_plan
{

enum class PhysicalStatementPlanKind
{
    Use,
    CreateDatabase,
    CreateCollection,
    CreateIndex,
    CreateVectorIndex,
    DropDatabase,
    DropCollection,
    DropIndex,
    DropVectorIndex,
    ShowDatabases,
    ShowCollections,
    ShowIndexes,
    ShowVectorIndexes,
    DescribeCollection,
    Insert,
    Update,
    Delete,
    Query,
};

class PhysicalStatementPlan
{
public:
    PhysicalStatementPlan(const PhysicalStatementPlan &) = delete;

    PhysicalStatementPlan & operator=(const PhysicalStatementPlan &) = delete;

    PhysicalStatementPlan(PhysicalStatementPlan &&) noexcept = default;

    PhysicalStatementPlan & operator=(PhysicalStatementPlan &&) noexcept = default;

    virtual ~PhysicalStatementPlan() noexcept = default;

protected:
    PhysicalStatementPlan(PhysicalStatementPlanKind kind, parser::ast::AstNodeLocation location) noexcept;

public:
    [[nodiscard]]
    PhysicalStatementPlanKind kind() const noexcept;

    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

private:
    PhysicalStatementPlanKind kind_;
    parser::ast::AstNodeLocation location_;
};

} // namespace litedb::core::physical_plan
