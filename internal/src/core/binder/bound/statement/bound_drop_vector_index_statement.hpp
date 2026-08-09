#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 DROP VINDEX 语句
class BoundDropVectorIndexStatement final : public BoundStatement
{
public:
    BoundDropVectorIndexStatement(std::optional<common::VIndexId> vector_index_id) noexcept;

public:
    // 获取向量索引 ID
    [[nodiscard]]
    std::optional<common::VIndexId> vector_index_id() const noexcept;

private:
    // vector_index_id_ 为 nullopt 时表示用户传入了重复向量索引名但是用了 IF EXISTS
    std::optional<common::VIndexId> vector_index_id_;
};

} // namespace litedb::core::binder::bound
