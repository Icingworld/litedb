#include "core/physical_plan/physical_planner.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/logical_plan/node/logical_filter.hpp"
#include "core/logical_plan/node/logical_limit.hpp"
#include "core/logical_plan/node/logical_order_by.hpp"
#include "core/logical_plan/node/logical_projection.hpp"
#include "core/logical_plan/node/logical_scan.hpp"
#include "core/logical_plan/statement/mutation/delete_plan.hpp"
#include "core/logical_plan/statement/mutation/update_plan.hpp"
#include "core/logical_plan/statement/query/query_plan.hpp"
#include "core/logical_plan/statement/statement_plan.hpp"
#include "core/physical_plan/node/physical_filter.hpp"
#include "core/physical_plan/node/physical_index_scan.hpp"
#include "core/physical_plan/node/physical_limit.hpp"
#include "core/physical_plan/node/physical_projection.hpp"
#include "core/physical_plan/node/physical_seq_scan.hpp"
#include "core/physical_plan/node/physical_sort.hpp"
#include "core/physical_plan/statement/physical_query_plan.hpp"
#include "core/physical_plan/statement/physical_row_mutation_plan.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

namespace
{

using planner::logical::LogicalFilter;
using planner::logical::LogicalIndexBound;
using planner::logical::LogicalIndexLookup;
using planner::logical::LogicalIndexLookupKind;
using planner::logical::LogicalLimit;
using planner::logical::LogicalOrderBy;
using planner::logical::LogicalPlanNode;
using planner::logical::LogicalPlanNodeKind;
using planner::logical::LogicalProjection;
using planner::logical::LogicalScan;
using planner::logical::LogicalScanIndexHint;
using planner::plan::DeletePlan;
using planner::plan::QueryPlan;
using planner::plan::StatementPlan;
using planner::plan::StatementPlanKind;
using planner::plan::UpdatePlan;

std::vector<binder::bound::BoundProjectionItem> clone_projections(
    const std::vector<binder::bound::BoundProjectionItem> & projections
)
{
    std::vector<binder::bound::BoundProjectionItem> cloned;
    cloned.reserve(projections.size());
    for (const auto & projection : projections) {
        cloned.push_back(binder::bound::BoundProjectionItem {
            .expression = projection.expression->clone(),
            .alias = projection.alias,
        });
    }
    return cloned;
}

std::vector<binder::bound::BoundOrderByItem> clone_order_by(
    const std::vector<binder::bound::BoundOrderByItem> & order_by
)
{
    std::vector<binder::bound::BoundOrderByItem> cloned;
    cloned.reserve(order_by.size());
    for (const auto & item : order_by) {
        cloned.push_back(binder::bound::BoundOrderByItem {
            .expression = item.expression->clone(),
            .ascending = item.ascending,
        });
    }
    return cloned;
}

PhysicalIndexLookupKind lower_lookup_kind(LogicalIndexLookupKind kind) noexcept
{
    switch (kind) {
    case LogicalIndexLookupKind::Equal:
        return PhysicalIndexLookupKind::Equal;
    case LogicalIndexLookupKind::Range:
        return PhysicalIndexLookupKind::Range;
    }
    return PhysicalIndexLookupKind::Equal;
}

std::optional<PhysicalIndexBound> lower_bound(const std::optional<LogicalIndexBound> & bound)
{
    if (!bound.has_value()) {
        return std::nullopt;
    }
    return PhysicalIndexBound {
        .key = bound->key,
        .inclusive = bound->inclusive,
    };
}

PhysicalIndexLookup lower_lookup(const LogicalIndexLookup & lookup)
{
    return PhysicalIndexLookup {
        .kind = lower_lookup_kind(lookup.kind),
        .lower = lower_bound(lookup.lower),
        .upper = lower_bound(lookup.upper),
    };
}

PhysicalStatementPlanKind lower_statement_kind(StatementPlanKind kind) noexcept
{
    switch (kind) {
    case StatementPlanKind::Use:
        return PhysicalStatementPlanKind::Use;
    case StatementPlanKind::CreateDatabase:
        return PhysicalStatementPlanKind::CreateDatabase;
    case StatementPlanKind::CreateCollection:
        return PhysicalStatementPlanKind::CreateCollection;
    case StatementPlanKind::CreateIndex:
        return PhysicalStatementPlanKind::CreateIndex;
    case StatementPlanKind::CreateVectorIndex:
        return PhysicalStatementPlanKind::CreateVectorIndex;
    case StatementPlanKind::DropDatabase:
        return PhysicalStatementPlanKind::DropDatabase;
    case StatementPlanKind::DropCollection:
        return PhysicalStatementPlanKind::DropCollection;
    case StatementPlanKind::DropIndex:
        return PhysicalStatementPlanKind::DropIndex;
    case StatementPlanKind::DropVectorIndex:
        return PhysicalStatementPlanKind::DropVectorIndex;
    case StatementPlanKind::ShowDatabases:
        return PhysicalStatementPlanKind::ShowDatabases;
    case StatementPlanKind::ShowCollections:
        return PhysicalStatementPlanKind::ShowCollections;
    case StatementPlanKind::ShowIndexes:
        return PhysicalStatementPlanKind::ShowIndexes;
    case StatementPlanKind::ShowVectorIndexes:
        return PhysicalStatementPlanKind::ShowVectorIndexes;
    case StatementPlanKind::DescribeCollection:
        return PhysicalStatementPlanKind::DescribeCollection;
    case StatementPlanKind::Insert:
        return PhysicalStatementPlanKind::Insert;
    case StatementPlanKind::Update:
        return PhysicalStatementPlanKind::Update;
    case StatementPlanKind::Delete:
        return PhysicalStatementPlanKind::Delete;
    case StatementPlanKind::Query:
        return PhysicalStatementPlanKind::Query;
    }
    return PhysicalStatementPlanKind::Query;
}

} // namespace

std::unique_ptr<PhysicalStatementPlan> PhysicalPlanner::plan(const StatementPlan & statement) const
{
    switch (statement.kind()) {
    case StatementPlanKind::Query: {
        const auto & query = static_cast<const QueryPlan &>(statement);
        return std::make_unique<PhysicalQueryPlan>(plan(query.root()), query.location());
    }
    case StatementPlanKind::Update: {
        const auto & update = static_cast<const UpdatePlan &>(statement);
        return std::make_unique<PhysicalRowMutationPlan>(
            PhysicalStatementPlanKind::Update,
            plan(update.input()),
            update.database_id(),
            update.collection_id(),
            update.collection_name(),
            update.location()
        );
    }
    case StatementPlanKind::Delete: {
        const auto & del = static_cast<const DeletePlan &>(statement);
        return std::make_unique<PhysicalRowMutationPlan>(
            PhysicalStatementPlanKind::Delete,
            plan(del.input()),
            del.database_id(),
            del.collection_id(),
            del.collection_name(),
            del.location()
        );
    }
    case StatementPlanKind::Use:
    case StatementPlanKind::CreateDatabase:
    case StatementPlanKind::CreateCollection:
    case StatementPlanKind::CreateIndex:
    case StatementPlanKind::CreateVectorIndex:
    case StatementPlanKind::DropDatabase:
    case StatementPlanKind::DropCollection:
    case StatementPlanKind::DropIndex:
    case StatementPlanKind::DropVectorIndex:
    case StatementPlanKind::ShowDatabases:
    case StatementPlanKind::ShowCollections:
    case StatementPlanKind::ShowIndexes:
    case StatementPlanKind::ShowVectorIndexes:
    case StatementPlanKind::DescribeCollection:
    case StatementPlanKind::Insert:
        return std::make_unique<PhysicalSimpleStatementPlan>(
            lower_statement_kind(statement.kind()),
            statement.location()
        );
    }

    return nullptr;
}

std::unique_ptr<PhysicalPlanNode> PhysicalPlanner::plan(const LogicalPlanNode & logical_root) const
{
    switch (logical_root.kind()) {
    case LogicalPlanNodeKind::Scan: {
        const auto & scan = static_cast<const LogicalScan &>(logical_root);
        if (scan.index_hint().has_value()) {
            const auto & hint = scan.index_hint().value();
            return std::make_unique<PhysicalIndexScan>(
                scan.database_id(),
                scan.collection_id(),
                scan.collection_name(),
                hint.index_id,
                hint.index_name,
                hint.index_kind,
                hint.column_id,
                hint.column_name,
                lower_lookup(hint.lookup),
                scan.location()
            );
        }
        return std::make_unique<PhysicalSeqScan>(
            scan.database_id(),
            scan.collection_id(),
            scan.collection_name(),
            scan.location()
        );
    }
    case LogicalPlanNodeKind::Filter: {
        const auto & filter = static_cast<const LogicalFilter &>(logical_root);
        return std::make_unique<PhysicalFilter>(
            plan(filter.child()),
            filter.predicate().clone(),
            filter.location()
        );
    }
    case LogicalPlanNodeKind::Projection: {
        const auto & projection = static_cast<const LogicalProjection &>(logical_root);
        return std::make_unique<PhysicalProjection>(
            plan(projection.child()),
            clone_projections(projection.projections()),
            projection.location()
        );
    }
    case LogicalPlanNodeKind::OrderBy: {
        const auto & order_by = static_cast<const LogicalOrderBy &>(logical_root);
        return std::make_unique<PhysicalSort>(
            plan(order_by.child()),
            clone_order_by(order_by.order_by()),
            order_by.location()
        );
    }
    case LogicalPlanNodeKind::Limit: {
        const auto & limit = static_cast<const LogicalLimit &>(logical_root);
        return std::make_unique<PhysicalLimit>(
            plan(limit.child()),
            limit.limit(),
            limit.offset(),
            limit.location()
        );
    }
    }

    return nullptr;
}

} // namespace litedb::core::physical_plan
