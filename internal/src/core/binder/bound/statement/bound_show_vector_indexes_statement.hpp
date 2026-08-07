#pragma once

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 SHOW VECTOR INDEXES 语句
 */
class BoundShowVectorIndexesStatement final : public BoundStatement
{
public:
    BoundShowVectorIndexesStatement(
        common::CollectionId collection_id
    ) noexcept;

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

private:
    common::CollectionId collection_id_;    // 集合 ID
};

} // namespace litedb::core::binder::bound
