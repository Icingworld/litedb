#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// 删除向量索引语句
class DropVectorIndexStatement final : public StatementNode
{
public:
    DropVectorIndexStatement(
        std::string vector_index_name,
        std::string collection_name,
        bool if_exists,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取向量索引名称
    [[nodiscard]]
    const std::string & vector_index_name() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 是否存在 IF EXISTS
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::string vector_index_name_;
    std::string collection_name_;
    bool if_exists_;
};

} // namespace litedb::core::parser::ast
