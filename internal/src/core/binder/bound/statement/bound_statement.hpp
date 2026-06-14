#pragma once

#include <cstdint>
#include <string>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::binder::bound
{

enum class BoundStatementKind : std::uint8_t
{
    Use,
    Select,
    Insert,
    Update,
    Delete,
    CreateDatabase,
    CreateCollection,
    CreateIndex,
    DropDatabase,
    DropCollection,
    DropIndex,
    AlterDatabase,
    AlterCollection,
    ShowDatabases,
    ShowCollections,
    DescribeCollection,
};

struct BoundColumn
{
    common::ColumnId column_id {0};
    std::string name;
    common::LogicalType type;
    bool nullable {true};
};

class BoundStatement
{
public:
    virtual ~BoundStatement() noexcept = default;

    BoundStatement(const BoundStatement &) = delete;
    BoundStatement & operator=(const BoundStatement &) = delete;
    BoundStatement(BoundStatement &&) noexcept = default;
    BoundStatement & operator=(BoundStatement &&) noexcept = default;

    [[nodiscard]]
    BoundStatementKind kind() const noexcept;

    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

protected:
    BoundStatement(BoundStatementKind kind, parser::ast::AstNodeLocation location) noexcept;

private:
    BoundStatementKind kind_;
    parser::ast::AstNodeLocation location_;
};

} // namespace litedb::core::binder::bound
