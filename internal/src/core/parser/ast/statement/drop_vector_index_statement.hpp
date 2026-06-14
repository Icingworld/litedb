#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

class DropVectorIndexStatement final : public StatementNode
{
public:
    DropVectorIndexStatement(
        std::string index_name,
        std::string collection_name,
        bool if_exists,
        AstNodeLocation location
    ) noexcept;

public:
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & index_name() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::string index_name_;
    std::string collection_name_;
    bool if_exists_;
};

} // namespace litedb::core::parser::ast
