#pragma once

#include <optional>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 DROP VINDEX 语句
 */
class BoundDropVectorIndexStatement final : public BoundStatement
{
public:
    BoundDropVectorIndexStatement(
        std::optional<common::VIndexId> vector_index_id
    ) noexcept;

public:
    /**
     * @brief 获取向量索引 ID
     * @return 向量索引 ID
     */
    [[nodiscard]]
    std::optional<common::VIndexId> vector_index_id() const noexcept;

private:
    std::optional<common::VIndexId> vector_index_id_;  ///< 向量索引 ID
};

} // namespace litedb::core::binder::bound
