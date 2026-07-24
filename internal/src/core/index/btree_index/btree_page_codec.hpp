#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <string>

#include "core/common/logical_type.hpp"
#include "core/index/btree_index/btree_page.hpp"

namespace litedb::core::index::btree_index
{

/**
 * @brief B+ 树页编解码错误码
 */
enum class BTreePageCodecErrorCode
{
    UnsupportedKeyType,      ///< 不支持的索引键类型
    KeyTypeMismatch,         ///< 页中的键与索引键类型不匹配
    InvalidPage,             ///< 逻辑页状态无效
    PageTooLarge,            ///< 逻辑页无法放入一个物理页
    InvalidFormat,           ///< 物理页格式无效
    UnsupportedVersion,      ///< 不支持的物理页格式版本
    ChecksumMismatch,        ///< 物理页校验和不匹配
    CorruptedPage,           ///< 物理页内容损坏
};

/**
 * @brief B+ 树页编解码错误
 */
struct BTreePageCodecError
{
    BTreePageCodecErrorCode code;     ///< 错误码
    std::string message;              ///< 错误信息
};

/**
 * @brief B+ 树逻辑页与固定大小物理页之间的编解码器
 * @details 编解码器只定义页内格式，不负责文件读写、页分配、缓存或树结构修改。
 */
class BTreePageCodec final
{
public:
    static constexpr std::size_t PageSize = 4096;       ///< 物理页大小
    static constexpr std::size_t HeaderSize = 48;       ///< 物理页头部大小
    static constexpr std::size_t SlotSize = 4;          ///< 物理页条目大小

    using PageBuffer = std::array<std::byte, PageSize>; ///< 物理页缓冲区类型

public:
    /**
     * @brief 计算逻辑页编码后的实际占用字节数
     */
    [[nodiscard]]
    static std::expected<std::size_t, BTreePageCodecError> encoded_size(
        const BTreePage & page,
        const common::LogicalType & key_type
    );

    /**
     * @brief 判断逻辑页是否能放入一个物理页
     */
    [[nodiscard]]
    static std::expected<bool, BTreePageCodecError> can_fit(
        const BTreePage & page,
        const common::LogicalType & key_type
    );

    /**
     * @brief 将逻辑页编码为固定大小物理页
     */
    [[nodiscard]]
    static std::expected<PageBuffer, BTreePageCodecError> encode(
        const BTreePage & page,
        const common::LogicalType & key_type
    );

    /**
     * @brief 将固定大小物理页解码为逻辑页
     * @param bytes 完整物理页字节
     * @param key_type 索引键类型
     * @param expected_page_id 调用方请求的页 ID
     */
    [[nodiscard]]
    static std::expected<BTreePage, BTreePageCodecError> decode(
        std::span<const std::byte> bytes,
        const common::LogicalType & key_type,
        BTreePageId expected_page_id
    );
};

} // namespace litedb::core::index::btree_index
