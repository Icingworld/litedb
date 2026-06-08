#include "core/planner/logical/logical_planner.hpp"

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

namespace litedb::core::planner::logical
{

namespace
{

using namespace litedb::core::binder::bound;

PlannerError error(
    PlannerErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
)
{
    return PlannerError {
        .code = code,
        .location = location,
        .message = std::move(message),
    };
}

std::unique_ptr<LogicalPlanNode> plan_select(BoundSelectStatement & statement)
{
    auto plan = std::make_unique<LogicalScan>(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    std::unique_ptr<LogicalPlanNode> current = std::move(plan);
    if (auto where = statement.take_where(); where != nullptr) {
        current = std::make_unique<LogicalFilter>(std::move(current), std::move(where), statement.location());
    }

    current = std::make_unique<LogicalProjection>(
        std::move(current),
        statement.take_projections(),
        statement.location()
    );

    auto order_by = statement.take_order_by();
    if (!order_by.empty()) {
        current = std::make_unique<LogicalOrderBy>(std::move(current), std::move(order_by), statement.location());
    }

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

std::unique_ptr<LogicalPlanNode> scan_for(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    const std::string & collection_name,
    parser::ast::AstNodeLocation location
)
{
    return std::make_unique<LogicalScan>(database_id, collection_id, collection_name, location);
}

std::unique_ptr<LogicalPlanNode> plan_update(BoundUpdateStatement & statement)
{
    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    if (auto where = statement.take_where(); where != nullptr) {
        current = std::make_unique<LogicalFilter>(std::move(current), std::move(where), statement.location());
    }

    return std::make_unique<LogicalUpdate>(
        std::move(current),
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.take_assignments(),
        statement.location()
    );
}

std::unique_ptr<LogicalPlanNode> plan_delete(BoundDeleteStatement & statement)
{
    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    if (auto where = statement.take_where(); where != nullptr) {
        current = std::make_unique<LogicalFilter>(std::move(current), std::move(where), statement.location());
    }

    return std::make_unique<LogicalDelete>(
        std::move(current),
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );
}

} // namespace

std::expected<std::unique_ptr<LogicalPlanNode>, PlannerError> LogicalPlanner::plan(
    std::unique_ptr<BoundStatement> statement
) const
{
    if (statement == nullptr) {
        return std::unexpected(error(
            PlannerErrorCode::InvalidArgument,
            parser::ast::AstNodeLocation {},
            "cannot plan a null bound statement"
        ));
    }

    switch (statement->kind()) {
    case BoundStatementKind::Use: {
        auto & use = static_cast<BoundUseStatement &>(*statement);
        return std::make_unique<LogicalUse>(use.database_id(), use.database_name(), use.location());
    }
    case BoundStatementKind::Select:
        return plan_select(static_cast<BoundSelectStatement &>(*statement));
    case BoundStatementKind::Insert: {
        auto & insert = static_cast<BoundInsertStatement &>(*statement);
        return std::make_unique<LogicalInsert>(
            insert.database_id(),
            insert.collection_id(),
            insert.collection_name(),
            insert.columns(),
            insert.take_values(),
            insert.location()
        );
    }
    case BoundStatementKind::Update:
        return plan_update(static_cast<BoundUpdateStatement &>(*statement));
    case BoundStatementKind::Delete:
        return plan_delete(static_cast<BoundDeleteStatement &>(*statement));
    case BoundStatementKind::CreateDatabase: {
        auto & create = static_cast<BoundCreateDatabaseStatement &>(*statement);
        return std::make_unique<LogicalCreateDatabase>(
            create.database_name(),
            create.if_not_exists(),
            create.location()
        );
    }
    case BoundStatementKind::CreateCollection: {
        auto & create = static_cast<BoundCreateCollectionStatement &>(*statement);
        return std::make_unique<LogicalCreateCollection>(
            create.database_id(),
            create.collection_name(),
            create.if_not_exists(),
            create.columns(),
            create.location()
        );
    }
    case BoundStatementKind::DropDatabase: {
        auto & drop = static_cast<BoundDropDatabaseStatement &>(*statement);
        return std::make_unique<LogicalDropDatabase>(
            drop.database_id(),
            drop.database_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case BoundStatementKind::DropCollection: {
        auto & drop = static_cast<BoundDropCollectionStatement &>(*statement);
        return std::make_unique<LogicalDropCollection>(
            drop.database_id(),
            drop.collection_id(),
            drop.collection_name(),
            drop.if_exists(),
            drop.location()
        );
    }
    case BoundStatementKind::ShowDatabases: {
        auto & show = static_cast<BoundShowDatabasesStatement &>(*statement);
        return std::make_unique<LogicalShowDatabases>(show.location());
    }
    case BoundStatementKind::ShowCollections: {
        auto & show = static_cast<BoundShowCollectionsStatement &>(*statement);
        return std::make_unique<LogicalShowCollections>(show.database_id(), show.location());
    }
    case BoundStatementKind::DescribeCollection: {
        auto & describe = static_cast<BoundDescribeCollectionStatement &>(*statement);
        return std::make_unique<LogicalDescribeCollection>(
            describe.database_id(),
            describe.collection_id(),
            describe.collection_name(),
            describe.location()
        );
    }
    case BoundStatementKind::AlterDatabase:
    case BoundStatementKind::AlterCollection:
        return std::unexpected(error(
            PlannerErrorCode::UnsupportedStatement,
            statement->location(),
            "unsupported bound statement kind"
        ));
    }

    return std::unexpected(error(
        PlannerErrorCode::UnsupportedStatement,
        statement->location(),
        "unknown bound statement kind"
    ));
}

} // namespace litedb::core::planner::logical
