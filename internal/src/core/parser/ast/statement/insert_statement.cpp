#include "core/parser/ast/statement/insert_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

InsertStatement::InsertStatement(
    std::string collection_name,
    ColumnList columns,
    ValueList values,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , columns_(std::move(columns))
    , values_(std::move(values))
{
}

AstNodeKind InsertStatement::kind() const noexcept
{
    return AstNodeKind::Insert;
}

const std::string & InsertStatement::collection_name() const noexcept
{
    return collection_name_;
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
