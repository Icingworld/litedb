#pragma once

#include <expected>

#include "core/schema/value.hpp"
#include "core/vindex/vector_index_error.hpp"

namespace litedb::core::vindex
{

/**
 * @brief 从逻辑值提取向量索引键
 * @param value 逻辑值
 * @return 向量值
 */
[[nodiscard]]
std::expected<schema::VectorValue, VectorIndexError> vector_key_from_value(const schema::Value & value);

} // namespace litedb::core::vindex
