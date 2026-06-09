#pragma once

#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::schema
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
    common::RecordId record_id {0};     ///< 记录 ID
    RecordData data;                    ///< 记录数据
};

} // namespace litedb::core::schema
