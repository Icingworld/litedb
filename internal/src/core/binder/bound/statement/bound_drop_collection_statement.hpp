#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定 DROP COLLECTION 语句
class BoundDropCollectionStatement final : public BoundStatement
{
public:
    BoundDropCollectionStatement(std::optional<common::CollectionId> collection_id) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    std::optional<common::CollectionId> collection_id() const noexcept;

private:
    // collection_id_ 为 nullopt 时表示用户传入了重复集合名但是用了 IF EXISTS
    std::optional<common::CollectionId> collection_id_;
};

} // namespace litedb::core::binder::bound
