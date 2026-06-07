#pragma once

#include <cstdint>

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定语句类型
 */
enum class BoundStatementKind : std::uint8_t
{
    Use,
    Select,
    Insert,
    Update,
    Delete,
    CreateDatabase,
    CreateCollection,
    DropDatabase,
    DropCollection,
    AlterDatabase,
    AlterCollection,
    ShowDatabases,
    ShowCollections
};

/**
 * @brief 绑定语句
 */
class BoundStatement
{
public:
    virtual ~BoundStatement() noexcept = default;

    BoundStatement(const BoundStatement &) = delete;

    BoundStatement & operator=(const BoundStatement &) = delete;

    BoundStatement(BoundStatement &&) noexcept = default;

    BoundStatement & operator=(BoundStatement &&) noexcept = default;

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
    BoundStatementKind kind_;   ///< 绑定语句类型
};

} // namespace litedb::core::binder::bound
