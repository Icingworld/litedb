#pragma once

#include <vector>

#include "core/common/ids.hpp"
#include "core/common/value.hpp"

namespace litedb::core::common
{

/**
 * @brief 记录数据
 */
struct RecordData
{
    std::vector<Value> values;  ///< 按 collection schema 列顺序排列的值
};

/**
 * @brief 记录
 */
struct Record
{
    RecordId record_id {0};     ///< 记录 ID
    RecordData data;            ///< 记录数据
};

} // namespace litedb::core::common
