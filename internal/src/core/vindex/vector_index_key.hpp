#pragma once

#include <cstddef>
#include <expected>

#include "core/common/value.hpp"
#include "core/vindex/vector_index_error.hpp"

namespace litedb::core::vindex
{

/**
 * @brief 向量索引键
 */
class VectorIndexKey
{
private:
    explicit VectorIndexKey(common::VectorValue vector);

public:
    /**
     * @brief 从逻辑值创建向量索引键
     * @param value 逻辑值
     * @return 向量索引键
     */
    [[nodiscard]]
    static std::expected<VectorIndexKey, VectorIndexError> from_value(
        const common::Value & value
    );

    /**
     * @brief 从向量值创建向量索引键
     * @param vector 向量值
     * @return 向量索引键
     */
    [[nodiscard]]
    static std::expected<VectorIndexKey, VectorIndexError> from_vector(
        common::VectorValue vector
    );

    /**
     * @brief 获取向量值
     * @return 向量值
     */
    [[nodiscard]]
    const common::VectorValue & value() const noexcept;

    /**
     * @brief 获取向量维度
     * @return 向量维度
     */
    [[nodiscard]]
    std::size_t dimension() const noexcept;

private:
    common::VectorValue value_;             // 向量值
};

} // namespace litedb::core::vindex
