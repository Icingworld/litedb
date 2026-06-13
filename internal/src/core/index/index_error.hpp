#pragma once

#include <string>

namespace litedb::core::index
{

/**
 * @brief 索引错误码
 */
enum class IndexErrorCode
{
    UnsupportedKeyType,     ///< 不支持的键类型
    UnsupportedRangeScan,   ///< 不支持的范围扫描
    KeyNotFound,            ///< 键不存在
    RecordNotFound,         ///< 记录不存在
};

/**
 * @brief 索引错误
 */
struct IndexError
{
    IndexErrorCode code;    ///< 错误码
    std::string message;    ///< 错误信息
};

} // namespace litedb::core::index
