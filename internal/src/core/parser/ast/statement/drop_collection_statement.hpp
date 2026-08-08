#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// DROP COLLECTION 语句节点
class DropCollectionStatement final : public StatementNode
{
public:
    DropCollectionStatement(
        std::string collection_name,
        bool if_exists,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 是否存在 IF EXISTS
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::string collection_name_;   // 集合名称
    bool if_exists_;                // 是否存在 IF EXISTS
};

} // namespace litedb::core::parser::ast
