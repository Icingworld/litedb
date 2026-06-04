#pragma once

#include <string>

#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

class CreateCollectionStatement final : public StatementNode
{
public:
    CreateCollectionStatement(
        std::string collection,
        bool if_not_exists,
        ColumnDefinitionList columns,
        AstNodeLocation location
    ) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & collection() const noexcept;

    [[nodiscard]]
    bool if_not_exists() const noexcept;

    [[nodiscard]]
    const ColumnDefinitionList & columns() const noexcept;

private:
    std::string collection_;
    bool if_not_exists_;
    ColumnDefinitionList columns_;
};

} // namespace litedb::core::parser::ast
