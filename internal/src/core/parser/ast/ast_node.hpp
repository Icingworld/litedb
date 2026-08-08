#pragma once

#include <cstddef>
#include <cstdint>

namespace litedb::core::parser::ast
{

// 抽象语法树节点位置
struct AstNodeLocation
{
    std::size_t line; // 行号
    std::size_t column; // 列号
};

// 抽象语法树节点类型
enum class AstNodeKind : std::uint8_t
{
    CreateDatabase,
    CreateCollection,
    CreateIndex,
    CreateVectorIndex,
    Delete,
    DescribeCollection,
    DropDatabase,
    DropCollection,
    DropIndex,
    DropVectorIndex,
    Insert,
    Select,
    ShowDatabases,
    ShowCollections,
    ShowIndexes,
    ShowVectorIndexes,
    Update,
    Use,

    Wildcard, // *
    Literal, // 字面量
    FunctionCall, // 函数调用
    ColumnReference, // 列引用
    Vector,
    Binary, // 二元运算符
    Unary, // 一元运算符
    In,
    Between,
    Like,
    Alias, // AS alias
};

// 抽象语法树节点
class AstNode
{
protected:
    explicit AstNode(AstNodeLocation location) noexcept;

public:
    AstNode(const AstNode &) = delete;

    AstNode & operator=(const AstNode &) = delete;

    AstNode(AstNode &&) noexcept = default;

    AstNode & operator=(AstNode &&) noexcept = default;

    virtual ~AstNode() noexcept = default;

public:
    // 获取节点位置
    [[nodiscard]]
    AstNodeLocation location() const noexcept;

    // 获取节点类型
    [[nodiscard]]
    virtual AstNodeKind kind() const noexcept = 0;

private:
    AstNodeLocation location_;
};

} // namespace litedb::core::parser::ast
