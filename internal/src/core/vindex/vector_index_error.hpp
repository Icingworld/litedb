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
    UnsupportedIndexKind,   ///< 不支持的向量索引类型
    InvalidDimension,       ///< 向量维度无效
    EmptyQuery,             ///< 查询向量为空
    RecordAlreadyExists,    ///< 记录已存在
    RecordNotFound,         ///< 记录不存在
    IndexAlreadyExists,     ///< 索引已存在
    IndexNotFound,          ///< 索引不存在
    InvalidMetadata,        ///< 索引元数据或目标列无效
    IndexFileMissing,       ///< 持久化索引文件不存在
    CorruptedIndex,         ///< 持久化索引内容损坏或不兼容
    StaleIndex,             ///< 持久化索引与集合数据不一致
    FileSystemFailure,      ///< 文件系统操作失败
    StorageFailure,         ///< 存储扫描失败
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
