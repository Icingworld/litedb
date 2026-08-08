#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// 创建索引方法
enum class CreateIndexMethod
{
    Default, // 默认
    BTree, // B+ 树
};

// CREATE INDEX 语句节点
class CreateIndexStatement final : public StatementNode
{
public:
    CreateIndexStatement(
        std::string index_name,
        std::string collection_name,
        std::string column_name,
        bool if_not_exists,
        bool unique,
        CreateIndexMethod method,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取索引名称
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 获取列名称
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    // 是否存在 IF NOT EXISTS
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    // 是否为唯一索引
    [[nodiscard]]
    bool unique() const noexcept;

    // 获取创建索引方法
    [[nodiscard]]
    CreateIndexMethod method() const noexcept;

private:
    std::string index_name_;
    std::string collection_name_;
    std::string column_name_;
    bool if_not_exists_;
    bool unique_;
    CreateIndexMethod method_;
};

} // namespace litedb::core::parser::ast
