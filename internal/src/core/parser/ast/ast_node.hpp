#pragma once

#include <cstddef>
#include <cstdint>

#include "core/parser/ast/visitor.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 抽象语法树节点位置
 */
struct AstNodeLocation
{
    std::size_t line;       ///< 行号
    std::size_t column;     ///< 列号
};

/**
 * @brief 抽象语法树节点类型
 */
enum class AstNodeKind : std::uint8_t
{
    Alter,                  ///< ALTER
    CreateDatabase,         ///< CREATE DATABASE
    CreateCollection,       ///< CREATE COLLECTION
    CreateIndex,            ///< CREATE INDEX
    CreateVectorIndex,      ///< CREATE VINDEX
    Delete,                 ///< DELETE
    Describe,               ///< DESCRIBE
    DropDatabase,           ///< DROP DATABASE
    DropCollection,         ///< DROP COLLECTION
    DropIndex,              ///< DROP INDEX
    DropVectorIndex,        ///< DROP VINDEX
    Insert,                 ///< INSERT
    Select,                 ///< SELECT
    ShowDatabases,          ///< SHOW DATABASES
    ShowCollections,        ///< SHOW COLLECTIONS
    ShowIndexes,            ///< SHOW INDEXES
    ShowVectorIndexes,      ///< SHOW VINDEXES
    Update,                 ///< UPDATE
    Use,                    ///< USE

    Identifier,             ///< 标识符
    Wildcard,               ///< *
    Literal,                ///< 字面量
    FunctionCall,           ///< 函数调用
    ColumnReference,        ///< 列引用
    Vector,                 ///< 向量
    Binary,                 ///< 二元运算符
    Unary,                  ///< 一元运算符
    In,                     ///< IN
    Between,                ///< BETWEEN
    Like,                   ///< LIKE
    Alias,                  ///< AS alias
};

/**
 * @brief 抽象语法树节点
 * @note 所有节点都继承自此基类，不允许拷贝，只能移动
 */
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
    /**
     * @brief 获取节点位置
     * @return 节点位置
     */
    [[nodiscard]]
    AstNodeLocation location() const noexcept;

    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    virtual AstNodeKind kind() const noexcept = 0;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    virtual void accept(AstNodeVisitor & visitor) const = 0;

private:
    AstNodeLocation location_;  ///< 节点位置
};

} // namespace litedb::core::parser::ast
