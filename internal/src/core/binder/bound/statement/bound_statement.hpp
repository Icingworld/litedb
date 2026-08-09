#pragma once

#include <cstdint>

namespace litedb::core::binder::bound
{

// 绑定语句类型
enum class BoundStatementKind : std::uint8_t
{
    Use,
    Select,
    Insert,
    Update,
    Delete,
    CreateDatabase,
    CreateCollection,
    CreateIndex,
    CreateVectorIndex,
    DropDatabase,
    DropCollection,
    DropIndex,
    DropVectorIndex,
    ShowDatabases,
    ShowCollections,
    ShowIndexes,
    ShowVectorIndexes,
    DescribeCollection,
};

// 绑定语句基类
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
    // 获取绑定语句类型
    [[nodiscard]]
    BoundStatementKind kind() const noexcept;

private:
    BoundStatementKind kind_;
};

} // namespace litedb::core::binder::bound
