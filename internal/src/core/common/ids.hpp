#pragma once

#include <cstdint>

namespace litedb::core::common
{

// 数据库 ID
using DatabaseId = std::uint64_t;

// 集合 ID
using CollectionId = std::uint64_t;

// 列 ID
using ColumnId = std::uint64_t;

// 记录 ID
using RecordId = std::uint64_t;

// 索引 ID
using IndexId = std::uint64_t;

// 向量索引 ID
using VIndexId = std::uint64_t;

// 元数据项 ID
using MetaEntryId = std::uint64_t;

} // namespace litedb::core::common
