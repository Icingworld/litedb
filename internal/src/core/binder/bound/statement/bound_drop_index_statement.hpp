#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 DROP INDEX 语句
class BoundDropIndexStatement final : public BoundStatement
{
public:
    BoundDropIndexStatement(std::optional<common::IndexId> index_id) noexcept;

public:
    // 获取索引 ID
    [[nodiscard]]
    std::optional<common::IndexId> index_id() const noexcept;

private:
    // index_id_ 为 nullopt 时表示用户传入了重复索引名但是用了 IF EXISTS
    std::optional<common::IndexId> index_id_;
};

} // namespace litedb::core::binder::bound
