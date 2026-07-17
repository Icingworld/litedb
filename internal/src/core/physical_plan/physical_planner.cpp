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
#include "core/logical_plan/node/logical_vector_search.hpp"
#include "core/logical_plan/statement/command/create_collection_plan.hpp"
#include "core/logical_plan/statement/command/create_database_plan.hpp"
#include "core/logical_plan/statement/command/create_index_plan.hpp"
#include "core/logical_plan/statement/command/create_vector_index_plan.hpp"
#include "core/logical_plan/statement/command/describe_collection_plan.hpp"
#include "core/logical_plan/statement/command/drop_collection_plan.hpp"
#include "core/logical_plan/statement/command/drop_database_plan.hpp"
#include "core/logical_plan/statement/command/drop_index_plan.hpp"
#include "core/logical_plan/statement/command/drop_vector_index_plan.hpp"
#include "core/logical_plan/statement/command/show_collections_plan.hpp"
#include "core/logical_plan/statement/command/show_databases_plan.hpp"
#include "core/logical_plan/statement/command/show_indexes_plan.hpp"
#include "core/logical_plan/statement/command/show_vector_indexes_plan.hpp"
#include "core/logical_plan/statement/command/use_plan.hpp"
#include "core/logical_plan/statement/mutation/delete_plan.hpp"
#include "core/logical_plan/statement/mutation/insert_plan.hpp"
#include "core/logical_plan/statement/mutation/update_plan.hpp"
#include "core/logical_plan/statement/query/query_plan.hpp"
#include "core/logical_plan/statement/logical_statement_plan.hpp"
#include "core/physical_plan/node/physical_filter.hpp"
#include "core/physical_plan/node/physical_index_scan.hpp"
#include "core/physical_plan/node/physical_limit.hpp"
#include "core/physical_plan/node/physical_projection.hpp"
#include "core/physical_plan/node/physical_seq_scan.hpp"
#include "core/physical_plan/node/physical_sort.hpp"
#include "core/physical_plan/node/physical_vector_search.hpp"
#include "core/physical_plan/statement/physical_command_plan.hpp"
#include "core/physical_plan/statement/physical_insert_plan.hpp"
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
using planner::logical::LogicalVectorSearch;
using planner::plan::CreateCollectionPlan;
using planner::plan::CreateDatabasePlan;
using planner::plan::CreateIndexPlan;
using planner::plan::CreateVectorIndexPlan;
using planner::plan::DeletePlan;
using planner::plan::DescribeCollectionPlan;
using planner::plan::DropCollectionPlan;
using planner::plan::DropDatabasePlan;
using planner::plan::DropIndexPlan;
using planner::plan::DropVectorIndexPlan;
using planner::plan::InsertPlan;
using planner::plan::QueryPlan;
using planner::plan::ShowCollectionsPlan;
using planner::plan::ShowDatabasesPlan;
using planner::plan::ShowIndexesPlan;
using planner::plan::ShowVectorIndexesPlan;
using planner::plan::LogicalStatementPlan;
using planner::plan::LogicalStatementPlanKind;
using planner::plan::UpdatePlan;
using planner::plan::UsePlan;

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

std::vector<std::unique_ptr<binder::bound::BoundExpression>> clone_expressions(
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & expressions
)
{
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> cloned;
    cloned.reserve(expressions.size());
    for (const auto & expression : expressions) {
        cloned.push_back(expression->clone());
    }
    return cloned;
}

std::vector<binder::bound::BoundAssignment> clone_assignments(
    const std::vector<binder::bound::BoundAssignment> & assignments
)
{
    std::vector<binder::bound::BoundAssignment> cloned;
    cloned.reserve(assignments.size());
    for (const auto & assignment : assignments) {
        cloned.push_back(binder::bound::BoundAssignment {
            .column = assignment.column,
            .value = assignment.value->clone(),
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

} // namespace

std::unique_ptr<PhysicalStatementPlan> PhysicalPlanner::plan(const LogicalStatementPlan & statement) const
{
    switch (statement.kind()) {
    case LogicalStatementPlanKind::Query: {
        const auto & query = static_cast<const QueryPlan &>(statement);
        return std::make_unique<PhysicalQueryPlan>(plan(query.root()), query.location());
    }
    case LogicalStatementPlanKind::Update: {
        const auto & update = static_cast<const UpdatePlan &>(statement);
        return std::make_unique<PhysicalUpdatePlan>(
            plan(update.input()),
            update.database_id(),
            update.collection_id(),
            update.collection_name(),
            clone_assignments(update.assignments()),
            update.location()
        );
    }
    case LogicalStatementPlanKind::Delete: {
        const auto & del = static_cast<const DeletePlan &>(statement);
        return std::make_unique<PhysicalDeletePlan>(
            plan(del.input()),
            del.database_id(),
            del.collection_id(),
            del.collection_name(),
            del.location()
        );
    }
    case LogicalStatementPlanKind::Use: {
        const auto & use = static_cast<const UsePlan &>(statement);
        return std::make_unique<PhysicalUsePlan>(use.database_id(), use.database_name(), use.location());
    }
    case LogicalStatementPlanKind::CreateDatabase: {
        const auto & create = static_cast<const CreateDatabasePlan &>(statement);
        return std::make_unique<PhysicalCreateDatabasePlan>(
            create.database_name(),
            create.if_not_exists(),
            create.location()
        );
    }
    case LogicalStatementPlanKind::CreateCollection: {
        const auto & create = static_cast<const CreateCollectionPlan &>(statement);
        return std::make_unique<PhysicalCreateCollectionPlan>(
            create.database_id(),
            create.collection_name(),
            create.if_not_exists(),
            create.columns(),
            create.comment(),
            create.location()
        );
    }
    case LogicalStatementPlanKind::CreateIndex: {
        const auto & create = static_cast<const CreateIndexPlan &>(statement);
        return std::make_unique<PhysicalCreateIndexPlan>(
            create.database_id(),
            create.collection_id(),
            create.collection_name(),
            create.column_id(),
            create.column_name(),
            create.index_name(),
            create.index_kind(),
            create.unique(),
            create.if_not_exists(),
            create.location()
        );
    }
    case LogicalStatementPlanKind::CreateVectorIndex: {
        const auto & create = static_cast<const CreateVectorIndexPlan &>(statement);
        return std::make_unique<PhysicalCreateVectorIndexPlan>(
            create.database_id(),
            create.collection_id(),
            create.collection_name(),
            create.column_id(),
            create.column_name(),
            create.index_name(),
            create.index_kind(),
            create.metric(),
            create.max_neighbors(),
            create.ef_construction(),
            create.ef_search_default(),
            create.random_seed(),
            create.if_not_exists(),
            create.location()
        );
    }
    case LogicalStatementPlanKind::DropDatabase: {
        const auto & drop = static_cast<const DropDatabasePlan &>(statement);
        return std::make_unique<PhysicalDropDatabasePlan>(
            drop.database_id(),
            drop.database_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case LogicalStatementPlanKind::DropCollection: {
        const auto & drop = static_cast<const DropCollectionPlan &>(statement);
        return std::make_unique<PhysicalDropCollectionPlan>(
            drop.database_id(),
            drop.collection_id(),
            drop.collection_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case LogicalStatementPlanKind::DropIndex: {
        const auto & drop = static_cast<const DropIndexPlan &>(statement);
        return std::make_unique<PhysicalDropIndexPlan>(
            drop.database_id(),
            drop.collection_id(),
            drop.collection_name(),
            drop.index_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case LogicalStatementPlanKind::DropVectorIndex: {
        const auto & drop = static_cast<const DropVectorIndexPlan &>(statement);
        return std::make_unique<PhysicalDropVectorIndexPlan>(
            drop.database_id(),
            drop.collection_id(),
            drop.collection_name(),
            drop.index_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case LogicalStatementPlanKind::ShowDatabases: {
        const auto & show = static_cast<const ShowDatabasesPlan &>(statement);
        return std::make_unique<PhysicalShowDatabasesPlan>(show.location());
    }
    case LogicalStatementPlanKind::ShowCollections: {
        const auto & show = static_cast<const ShowCollectionsPlan &>(statement);
        return std::make_unique<PhysicalShowCollectionsPlan>(show.database_id(), show.location());
    }
    case LogicalStatementPlanKind::ShowIndexes: {
        const auto & show = static_cast<const ShowIndexesPlan &>(statement);
        return std::make_unique<PhysicalShowIndexesPlan>(
            show.database_id(),
            show.collection_id(),
            show.collection_name(),
            show.location()
        );
    }
    case LogicalStatementPlanKind::ShowVectorIndexes: {
        const auto & show = static_cast<const ShowVectorIndexesPlan &>(statement);
        return std::make_unique<PhysicalShowVectorIndexesPlan>(
            show.database_id(),
            show.collection_id(),
            show.collection_name(),
            show.location()
        );
    }
    case LogicalStatementPlanKind::DescribeCollection: {
        const auto & describe = static_cast<const DescribeCollectionPlan &>(statement);
        return std::make_unique<PhysicalDescribeCollectionPlan>(
            describe.database_id(),
            describe.collection_id(),
            describe.collection_name(),
            describe.location()
        );
    }
    case LogicalStatementPlanKind::Insert: {
        const auto & insert = static_cast<const InsertPlan &>(statement);
        return std::make_unique<PhysicalInsertPlan>(
            insert.database_id(),
            insert.collection_id(),
            insert.collection_name(),
            insert.columns(),
            clone_expressions(insert.values()),
            insert.location()
        );
    }
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
    case LogicalPlanNodeKind::VectorSearch: {
        const auto & search = static_cast<const LogicalVectorSearch &>(logical_root);
        return std::make_unique<PhysicalVectorSearch>(
            search.database_id(), search.collection_id(), search.collection_name(), search.index_id(),
            search.index_name(), search.column_id(), search.column_name(), search.metric(),
            search.query_vector().clone(), search.predicate() ? search.predicate()->clone() : nullptr,
            search.required_count(), search.location()
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
