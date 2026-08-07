#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 DROP INDEX 语句
 */
class BoundDropIndexStatement final : public BoundStatement
{
public:
    BoundDropIndexStatement(
        std::optional<common::IndexId> index_id
    ) noexcept;

public:
    /**
     * @brief 获取索引 ID
     * @return 索引 ID
     */
    [[nodiscard]]
    std::optional<common::IndexId> index_id() const noexcept;

private:
    std::optional<common::IndexId> index_id_;  // 索引 ID
};

} // namespace litedb::core::binder::bound
