#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// DESCRIBE COLLECTION 语句节点
class DescribeCollectionStatement final : public StatementNode
{
public:
    DescribeCollectionStatement(
        std::string collection_name,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    std::string collection_name_;       // 集合名称
};

} // namespace litedb::core::parser::ast
