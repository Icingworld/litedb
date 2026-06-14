#pragma once

#include <string>

namespace litedb::core::vindex
{

/**
 * @brief 向量索引错误码
 */
enum class VectorIndexErrorCode
{
    UnsupportedMetric,      ///< 不支持的距离度量
    InvalidDimension,       ///< 向量维度无效
    EmptyQuery,             ///< 查询向量为空
    RecordAlreadyExists,    ///< 记录已存在
    RecordNotFound,         ///< 记录不存在
    IndexAlreadyExists,     ///< 索引已存在
    IndexNotFound,          ///< 索引不存在
};

/**
 * @brief 向量索引错误
 */
struct VectorIndexError
{
    VectorIndexErrorCode code;  ///< 错误码
    std::string message;        ///< 错误信息
};

} // namespace litedb::core::vindex
