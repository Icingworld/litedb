#pragma once

#include <vector>

#include "core/common/ids.hpp"
#include "core/common/value.hpp"

namespace litedb::core::common
{

// 记录数据
struct RecordData
{
    std::vector<Value> values; // 按 collection schema 列顺序排列的值
};

// 记录
struct Record
{
    RecordId record_id {0};
    RecordData data;
};

} // namespace litedb::core::common
