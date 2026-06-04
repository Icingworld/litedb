#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

class CreateDatabaseStatement final : public StatementNode
{
public:
    CreateDatabaseStatement(std::string database, bool if_not_exists, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & database() const noexcept;

    [[nodiscard]]
    bool if_not_exists() const noexcept;

private:
    std::string database_;
    bool if_not_exists_;
};

} // namespace litedb::core::parser::ast
