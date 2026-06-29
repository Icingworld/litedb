#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/parser/ast/ast_node.hpp"
#include "core/binder/bound/statement/bount_statement_visitor.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定语句类型
 */
enum class BoundStatementKind
{
    Use,                    ///< USE
    Select,                 ///< SELECT
    Insert,                 ///< INSERT
    Update,                 ///< UPDATE
    Delete,                 ///< DELETE
    CreateDatabase,         ///< CREATE DATABASE
    CreateCollection,       ///< CREATE COLLECTION
    CreateIndex,            ///< CREATE INDEX
    CreateVectorIndex,      ///< CREATE VINDEX
    DropDatabase,           ///< DROP DATABASE
    DropCollection,         ///< DROP COLLECTION
    DropIndex,              ///< DROP INDEX
    DropVectorIndex,        ///< DROP VINDEX
    ShowDatabases,          ///< SHOW DATABASES
    ShowCollections,        ///< SHOW COLLECTIONS
    ShowIndexes,            ///< SHOW INDEXES
    ShowVectorIndexes,      ///< SHOW VINDEXES
    DescribeCollection,     ///< DESCRIBE COLLECTION
};

/**
 * @brief 绑定列
 */
struct BoundColumn
{
    common::ColumnId column_id {0};     ///< 列 ID
    std::string name;                   ///< 列名称
    common::LogicalType type;           ///< 列类型
    bool nullable {true};               ///< 是否可为 NULL
};

/**
 * @brief 绑定语句基类
 */
class BoundStatement
{
public:
    BoundStatement(const BoundStatement &) = delete;

    BoundStatement & operator=(const BoundStatement &) = delete;

    BoundStatement(BoundStatement &&) noexcept = default;

    BoundStatement & operator=(BoundStatement &&) noexcept = default;

    virtual ~BoundStatement() noexcept = default;

public:
    /**
     * @brief 获取绑定语句类型
     * @return 绑定语句类型
     */
    [[nodiscard]]
    BoundStatementKind kind() const noexcept;

    /**
     * @brief 获取节点位置
     * @return 节点位置
     */
    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    virtual void accept(BoundStatementVisitor & visitor) const = 0;

protected:
    BoundStatement(BoundStatementKind kind, parser::ast::AstNodeLocation location) noexcept;

private:
    BoundStatementKind kind_;                   ///< 绑定语句类型
    parser::ast::AstNodeLocation location_;     ///< 节点位置
};

} // namespace litedb::core::binder::bound
