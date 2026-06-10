#pragma once

#include <cstdint>

namespace litedb::core::common
{

/**
 * @brief 数据库 ID
 */
using DatabaseId = std::uint64_t;

/**
 * @brief 集合 ID
 */
using CollectionId = std::uint64_t;

/**
 * @brief 列 ID
 */
using ColumnId = std::uint64_t;

/**
 * @brief 记录 ID
 */
using RecordId = std::uint64_t;

/**
 * @brief 索引 ID
 */
using IndexId = std::uint64_t;

/**
 * @brief 向量索引 ID
 */
using VIndexId = std::uint64_t;

} // namespace litedb::core::common
