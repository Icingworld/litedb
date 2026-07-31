#pragma once

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

protected:
    explicit BoundStatement(BoundStatementKind kind) noexcept;

public:
    /**
     * @brief 获取绑定语句类型
     * @return 绑定语句类型
     */
    [[nodiscard]]
    BoundStatementKind kind() const noexcept;

private:
    BoundStatementKind kind_;                   ///< 绑定语句类型
};

} // namespace litedb::core::binder::bound
