#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/catalog/catalog_writer.hpp"
#include "core/common/ids.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::planner::logical
{

enum class LogicalPlanNodeKind : std::uint8_t
{
    Use,
    CreateDatabase,
    CreateCollection,
    DropDatabase,
    DropCollection,
    ShowDatabases,
    ShowCollections,
    DescribeCollection,
    Insert,
    Update,
    Delete,
    Scan,
    Filter,
    Projection,
    OrderBy,
    Limit,
};

class LogicalPlanNode
{
public:
    virtual ~LogicalPlanNode() noexcept = default;

    LogicalPlanNode(const LogicalPlanNode &) = delete;
    LogicalPlanNode & operator=(const LogicalPlanNode &) = delete;
    LogicalPlanNode(LogicalPlanNode &&) noexcept = default;
    LogicalPlanNode & operator=(LogicalPlanNode &&) noexcept = default;

    [[nodiscard]]
    LogicalPlanNodeKind kind() const noexcept;

    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

protected:
    LogicalPlanNode(LogicalPlanNodeKind kind, parser::ast::AstNodeLocation location) noexcept;

private:
    LogicalPlanNodeKind kind_;
    parser::ast::AstNodeLocation location_;
};

class LogicalUse final : public LogicalPlanNode
{
public:
    LogicalUse(common::DatabaseId database_id, std::string database_name, parser::ast::AstNodeLocation location);

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    const std::string & database_name() const noexcept;

private:
    common::DatabaseId database_id_;
    std::string database_name_;
};

class LogicalCreateDatabase final : public LogicalPlanNode
{
public:
    LogicalCreateDatabase(std::string database_name, bool if_not_exists, parser::ast::AstNodeLocation location);

    [[nodiscard]]
    const std::string & database_name() const noexcept;

    [[nodiscard]]
    bool if_not_exists() const noexcept;

private:
    std::string database_name_;
    bool if_not_exists_;
};

class LogicalCreateCollection final : public LogicalPlanNode
{
public:
    LogicalCreateCollection(
        common::DatabaseId database_id,
        std::string collection_name,
        bool if_not_exists,
        std::vector<catalog::ColumnDefinition> columns,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    bool if_not_exists() const noexcept;

    [[nodiscard]]
    const std::vector<catalog::ColumnDefinition> & columns() const noexcept;

private:
    common::DatabaseId database_id_;
    std::string collection_name_;
    bool if_not_exists_;
    std::vector<catalog::ColumnDefinition> columns_;
};

class LogicalDropDatabase final : public LogicalPlanNode
{
public:
    LogicalDropDatabase(
        std::optional<common::DatabaseId> database_id,
        std::string database_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    std::optional<common::DatabaseId> database_id() const noexcept;

    [[nodiscard]]
    const std::string & database_name() const noexcept;

    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::optional<common::DatabaseId> database_id_;
    std::string database_name_;
    bool if_exists_;
};

class LogicalDropCollection final : public LogicalPlanNode
{
public:
    LogicalDropCollection(
        common::DatabaseId database_id,
        std::optional<common::CollectionId> collection_id,
        std::string collection_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    std::optional<common::CollectionId> collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    common::DatabaseId database_id_;
    std::optional<common::CollectionId> collection_id_;
    std::string collection_name_;
    bool if_exists_;
};

class LogicalShowDatabases final : public LogicalPlanNode
{
public:
    explicit LogicalShowDatabases(parser::ast::AstNodeLocation location);
};

class LogicalShowCollections final : public LogicalPlanNode
{
public:
    LogicalShowCollections(common::DatabaseId database_id, parser::ast::AstNodeLocation location);

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

private:
    common::DatabaseId database_id_;
};

class LogicalDescribeCollection final : public LogicalPlanNode
{
public:
    LogicalDescribeCollection(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

class LogicalScan final : public LogicalPlanNode
{
public:
    LogicalScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

class LogicalUnaryNode : public LogicalPlanNode
{
public:
    [[nodiscard]]
    const LogicalPlanNode & child() const noexcept;

protected:
    LogicalUnaryNode(
        LogicalPlanNodeKind kind,
        std::unique_ptr<LogicalPlanNode> child,
        parser::ast::AstNodeLocation location
    ) noexcept;

private:
    std::unique_ptr<LogicalPlanNode> child_;
};

class LogicalFilter final : public LogicalUnaryNode
{
public:
    LogicalFilter(
        std::unique_ptr<LogicalPlanNode> child,
        std::unique_ptr<binder::bound::BoundExpression> predicate,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    const binder::bound::BoundExpression & predicate() const noexcept;

private:
    std::unique_ptr<binder::bound::BoundExpression> predicate_;
};

class LogicalProjection final : public LogicalUnaryNode
{
public:
    LogicalProjection(
        std::unique_ptr<LogicalPlanNode> child,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> projections,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & projections() const noexcept;

private:
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> projections_;
};

class LogicalOrderBy final : public LogicalUnaryNode
{
public:
    LogicalOrderBy(
        std::unique_ptr<LogicalPlanNode> child,
        std::vector<binder::bound::BoundOrderByItem> order_by,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    const std::vector<binder::bound::BoundOrderByItem> & order_by() const noexcept;

private:
    std::vector<binder::bound::BoundOrderByItem> order_by_;
};

class LogicalLimit final : public LogicalUnaryNode
{
public:
    LogicalLimit(
        std::unique_ptr<LogicalPlanNode> child,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

private:
    std::optional<std::size_t> limit_;
    std::optional<std::size_t> offset_;
};

class LogicalInsert final : public LogicalPlanNode
{
public:
    LogicalInsert(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<binder::bound::BoundColumn> columns,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> values,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    const std::vector<binder::bound::BoundColumn> & columns() const noexcept;

    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & values() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::vector<binder::bound::BoundColumn> columns_;
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;
};

class LogicalUpdate final : public LogicalUnaryNode
{
public:
    LogicalUpdate(
        std::unique_ptr<LogicalPlanNode> child,
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<binder::bound::BoundAssignment> assignments,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    const std::vector<binder::bound::BoundAssignment> & assignments() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::vector<binder::bound::BoundAssignment> assignments_;
};

class LogicalDelete final : public LogicalUnaryNode
{
public:
    LogicalDelete(
        std::unique_ptr<LogicalPlanNode> child,
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

} // namespace litedb::core::planner::logical
