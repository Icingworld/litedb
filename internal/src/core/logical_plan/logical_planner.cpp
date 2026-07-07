#include "core/logical_plan/logical_planner.hpp"

#include <memory>
#include <string>
#include <utility>

#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_show_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_show_vector_indexes_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/logical_plan/logical_planner_helper.hpp"
#include "core/logical_plan/node/logical_filter.hpp"
#include "core/logical_plan/node/logical_limit.hpp"
#include "core/logical_plan/node/logical_order_by.hpp"
#include "core/logical_plan/node/logical_projection.hpp"
#include "core/logical_plan/node/logical_scan.hpp"
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

namespace litedb::core::planner::logical
{

namespace
{

using namespace litedb::core::binder::bound;
using namespace plan;

/**
 * @brief 生成扫描节点
 * @param database_id 数据库ID
 * @param collection_id 集合ID
 * @param collection_name 集合名称
 * @param location 位置
 * @return 逻辑计划节点
 */
std::unique_ptr<LogicalPlanNode> scan_for(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    parser::ast::AstNodeLocation location
)
{
    return std::make_unique<LogicalScan>(
        database_id,
        collection_id,
        std::move(collection_name),
        location
    );
}

/**
 * @brief 生成过滤节点
 * @param input 输入
 * @param predicate 谓词
 * @param location 位置
 * @return 逻辑计划节点
 */
std::unique_ptr<LogicalPlanNode> apply_optional_filter(
    std::unique_ptr<LogicalPlanNode> input,
    std::unique_ptr<BoundExpression> predicate,
    parser::ast::AstNodeLocation location
)
{
    if (predicate == nullptr) {
        return input;
    }

    return std::make_unique<LogicalFilter>(std::move(input), std::move(predicate), location);
}

} // namespace

std::expected<std::unique_ptr<StatementPlan>, PlannerError> LogicalPlanner::plan(
    std::unique_ptr<BoundStatement> statement
) const
{
    if (statement == nullptr) {
        return std::unexpected(make_planner_error(
            PlannerErrorCode::InvalidArgument,
            parser::ast::AstNodeLocation {},
            "cannot plan a null bound statement"
        ));
    }

    switch (statement->kind()) {
    case BoundStatementKind::Use: {
        auto & use = static_cast<BoundUseStatement &>(*statement);
        return std::make_unique<UsePlan>(use.database_id(), use.database_name(), use.location());
    }
    case BoundStatementKind::Select: {
        auto & select = static_cast<BoundSelectStatement &>(*statement);
        return std::make_unique<QueryPlan>(plan_select(select), select.location());
    }
    case BoundStatementKind::Insert: {
        auto & insert = static_cast<BoundInsertStatement &>(*statement);
        return std::make_unique<InsertPlan>(
            insert.database_id(),
            insert.collection_id(),
            insert.collection_name(),
            insert.columns(),
            insert.take_values(),
            insert.location()
        );
    }
    case BoundStatementKind::Update: {
        auto & update = static_cast<BoundUpdateStatement &>(*statement);
        return std::make_unique<UpdatePlan>(
            plan_update_input(update),
            update.database_id(),
            update.collection_id(),
            update.collection_name(),
            update.take_assignments(),
            update.location()
        );
    }
    case BoundStatementKind::Delete: {
        auto & del = static_cast<BoundDeleteStatement &>(*statement);
        return std::make_unique<DeletePlan>(
            plan_delete_input(del),
            del.database_id(),
            del.collection_id(),
            del.collection_name(),
            del.location()
        );
    }
    case BoundStatementKind::CreateDatabase: {
        auto & create = static_cast<BoundCreateDatabaseStatement &>(*statement);
        return std::make_unique<CreateDatabasePlan>(
            create.database_name(),
            create.if_not_exists(),
            create.location()
        );
    }
    case BoundStatementKind::CreateCollection: {
        auto & create = static_cast<BoundCreateCollectionStatement &>(*statement);
        return std::make_unique<CreateCollectionPlan>(
            create.database_id(),
            create.collection_name(),
            create.if_not_exists(),
            create.columns(),
            create.comment(),
            create.location()
        );
    }
    case BoundStatementKind::CreateIndex: {
        auto & create = static_cast<BoundCreateIndexStatement &>(*statement);
        return std::make_unique<CreateIndexPlan>(
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
    case BoundStatementKind::CreateVectorIndex: {
        auto & create = static_cast<BoundCreateVectorIndexStatement &>(*statement);
        return std::make_unique<CreateVectorIndexPlan>(
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
    case BoundStatementKind::DropDatabase: {
        auto & drop = static_cast<BoundDropDatabaseStatement &>(*statement);
        return std::make_unique<DropDatabasePlan>(
            drop.database_id(),
            drop.database_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case BoundStatementKind::DropCollection: {
        auto & drop = static_cast<BoundDropCollectionStatement &>(*statement);
        return std::make_unique<DropCollectionPlan>(
            drop.database_id(),
            drop.collection_id(),
            drop.collection_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case BoundStatementKind::DropIndex: {
        auto & drop = static_cast<BoundDropIndexStatement &>(*statement);
        return std::make_unique<DropIndexPlan>(
            drop.database_id(),
            drop.collection_id(),
            drop.collection_name(),
            drop.index_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case BoundStatementKind::DropVectorIndex: {
        auto & drop = static_cast<BoundDropVectorIndexStatement &>(*statement);
        return std::make_unique<DropVectorIndexPlan>(
            drop.database_id(),
            drop.collection_id(),
            drop.collection_name(),
            drop.index_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case BoundStatementKind::ShowDatabases: {
        auto & show = static_cast<BoundShowDatabasesStatement &>(*statement);
        return std::make_unique<ShowDatabasesPlan>(show.location());
    }
    case BoundStatementKind::ShowCollections: {
        auto & show = static_cast<BoundShowCollectionsStatement &>(*statement);
        return std::make_unique<ShowCollectionsPlan>(show.database_id(), show.location());
    }
    case BoundStatementKind::ShowIndexes: {
        auto & show = static_cast<BoundShowIndexesStatement &>(*statement);
        return std::make_unique<ShowIndexesPlan>(
            show.database_id(),
            show.collection_id(),
            show.collection_name(),
            show.location()
        );
    }
    case BoundStatementKind::ShowVectorIndexes: {
        auto & show = static_cast<BoundShowVectorIndexesStatement &>(*statement);
        return std::make_unique<ShowVectorIndexesPlan>(
            show.database_id(),
            show.collection_id(),
            show.collection_name(),
            show.location()
        );
    }
    case BoundStatementKind::DescribeCollection: {
        auto & describe = static_cast<BoundDescribeCollectionStatement &>(*statement);
        return std::make_unique<DescribeCollectionPlan>(
            describe.database_id(),
            describe.collection_id(),
            describe.collection_name(),
            describe.location()
        );
    }
    }

    [[unlikely]] return std::unexpected(make_planner_error(
        PlannerErrorCode::UnsupportedStatement,
        statement->location(),
        "unknown bound statement kind"
    ));
}

std::unique_ptr<LogicalPlanNode> LogicalPlanner::plan_select(BoundSelectStatement & statement) const
{
    // 自底向上构建逻辑计划

    // 创建 Scan 节点
    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    // 将 Scan 节点挂在 Filter 节点上
    current = apply_optional_filter(std::move(current), statement.take_where(), statement.location());

    // 将 Filter 节点挂在 Projection 节点上
    current = std::make_unique<LogicalProjection>(
        std::move(current),
        statement.take_projections(),
        statement.location()
    );

    // TODO: 考虑 Projection 节点和 Order By 节点的顺序
    // 区别在于 Physical Plan 在投影时是否丢弃非投影列，这可能会导致 Order By 无法访问排序项

    // 将 Projection 节点挂在 Order By 节点上
    auto order_by = statement.take_order_by();
    if (!order_by.empty()) {
        current = std::make_unique<LogicalOrderBy>(std::move(current), std::move(order_by), statement.location());
    }

    // 将 Order By 节点挂在 Limit 节点上
    if (statement.limit().has_value() || statement.offset().has_value()) {
        current = std::make_unique<LogicalLimit>(
            std::move(current),
            statement.limit(),
            statement.offset(),
            statement.location()
        );
    }

    return current;
}

std::unique_ptr<LogicalPlanNode> LogicalPlanner::plan_update_input(BoundUpdateStatement & statement) const
{
    // UPDATE 语句一般只包含 Filter 和 Scan 两部分

    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    return apply_optional_filter(std::move(current), statement.take_where(), statement.location());
}

std::unique_ptr<LogicalPlanNode> LogicalPlanner::plan_delete_input(BoundDeleteStatement & statement) const
{
    // DELETE 语句一般只包含 Filter 和 Scan 两部分

    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    return apply_optional_filter(std::move(current), statement.take_where(), statement.location());
}

} // namespace litedb::core::planner::logical
