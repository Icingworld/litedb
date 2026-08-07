#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 DROP COLLECTION 语句
 */
class BoundDropCollectionStatement final : public BoundStatement
{
public:
    BoundDropCollectionStatement(
        std::optional<common::CollectionId> collection_id
    ) noexcept;

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    std::optional<common::CollectionId> collection_id() const noexcept;

private:
    std::optional<common::CollectionId> collection_id_;         // 集合 ID
};

} // namespace litedb::core::binder::bound
