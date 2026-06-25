#include "core/parser/ast/statement/insert_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

InsertStatement::InsertStatement(
    std::string collection,
    ColumnList columns,
    ValueList values,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_(std::move(collection))
    , columns_(std::move(columns))
    , values_(std::move(values))
{
}

AstNodeKind InsertStatement::kind() const noexcept
{
    return AstNodeKind::Insert;
}

void InsertStatement::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const std::string & InsertStatement::collection() const noexcept
{
    return collection_;
}

const InsertStatement::ColumnList & InsertStatement::columns() const noexcept
{
    return columns_;
}

const InsertStatement::ValueList & InsertStatement::values() const noexcept
{
    return values_;
}

} // namespace litedb::core::parser::ast
