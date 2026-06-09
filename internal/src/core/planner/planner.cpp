#include "core/planner/planner.hpp"

#include <memory>
#include <string>
#include <utility>

#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/planner/statement/create_collection_plan.hpp"
#include "core/planner/statement/create_database_plan.hpp"
#include "core/planner/statement/delete_plan.hpp"
#include "core/planner/statement/describe_collection_plan.hpp"
#include "core/planner/statement/drop_collection_plan.hpp"
#include "core/planner/statement/drop_database_plan.hpp"
#include "core/planner/statement/insert_plan.hpp"
#include "core/planner/statement/query_plan.hpp"
#include "core/planner/statement/show_collections_plan.hpp"
#include "core/planner/statement/show_databases_plan.hpp"
#include "core/planner/statement/update_plan.hpp"
#include "core/planner/statement/use_plan.hpp"

namespace litedb::core::planner
{

namespace
{

using namespace litedb::core::binder::bound;

logical::PlannerError error(
    logical::PlannerErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
)
{
    return logical::PlannerError {
        .code = code,
        .location = location,
        .message = std::move(message),
    };
}

} // namespace

std::expected<std::unique_ptr<StatementPlan>, logical::PlannerError> Planner::plan(
    std::unique_ptr<BoundStatement> statement
) const
{
    if (statement == nullptr) {
        return std::unexpected(error(
            logical::PlannerErrorCode::InvalidArgument,
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
        return std::make_unique<QueryPlan>(logical_planner_.plan_select(select), select.location());
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
            logical_planner_.plan_update_input(update),
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
            logical_planner_.plan_delete_input(del),
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
    case BoundStatementKind::ShowDatabases: {
        auto & show = static_cast<BoundShowDatabasesStatement &>(*statement);
        return std::make_unique<ShowDatabasesPlan>(show.location());
    }
    case BoundStatementKind::ShowCollections: {
        auto & show = static_cast<BoundShowCollectionsStatement &>(*statement);
        return std::make_unique<ShowCollectionsPlan>(show.database_id(), show.location());
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
    case BoundStatementKind::AlterDatabase:
    case BoundStatementKind::AlterCollection:
        return std::unexpected(error(
            logical::PlannerErrorCode::UnsupportedStatement,
            statement->location(),
            "unsupported bound statement kind"
        ));
    }

    return std::unexpected(error(
        logical::PlannerErrorCode::UnsupportedStatement,
        statement->location(),
        "unknown bound statement kind"
    ));
}

} // namespace litedb::core::planner
