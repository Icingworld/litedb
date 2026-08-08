#include "core/parser/ast/statement/insert_statement.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

InsertStatement::InsertStatement(
    std::string collection_name,
    std::vector<std::string> columns,
    std::vector<std::unique_ptr<ExpressionNode>> values,
    AstNodeLocation location
)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , columns_(std::move(columns))
    , values_(std::move(values))
{
    assert(!collection_name_.empty());
    for (const auto & column : columns_) {
        assert(!column.empty());
    }
    assert(!values_.empty());
    for (const auto & value : values_) {
        assert(value != nullptr);
    }
    if (!columns_.empty()) {
        assert(columns_.size() == values_.size());
    }
}

AstNodeKind InsertStatement::kind() const noexcept
{
    return AstNodeKind::Insert;
}

const std::string & InsertStatement::collection_name() const noexcept
{
    return collection_name_;
}

const std::vector<std::string> & InsertStatement::columns() const noexcept
{
    return columns_;
}

const std::vector<std::unique_ptr<ExpressionNode>> & InsertStatement::values() const noexcept
{
    return values_;
}

} // namespace litedb::core::parser::ast
