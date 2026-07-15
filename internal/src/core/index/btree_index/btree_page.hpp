#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "core/common/ids.hpp"
#include "core/index/scalar_index_key.hpp"

namespace litedb::core::index::btree_index
{

/**
 * @brief B+ 树页 ID
 */
using BTreePageId = std::uint64_t;

/**
 * @brief 无效的 B+ 树页 ID
 */
inline constexpr BTreePageId InvalidBTreePageId = 0;

/**
 * @brief B+ 树页类型
 */
enum class BTreePageType
{
    Internal,       ///< 内部页
    Leaf,           ///< 叶子页
};

/**
 * @brief B+ 树内部排序键
 * @details 使用 (key, record_id) 作为全序键，使重复标量键跨页时仍能精确路由。
 */
struct BTreeEntryKey
{
    ScalarIndexKey key;             ///< 标量索引键
    common::RecordId record_id;     ///< 记录 ID
};

/**
 * @brief 比较两个 B+ 树内部排序键
 */
[[nodiscard]]
std::strong_ordering compare_btree_entry_keys(
    const BTreeEntryKey & left,
    const BTreeEntryKey & right
) noexcept;

/**
 * @brief B+ 树内部排序键小于比较器
 */
struct BTreeEntryKeyLess
{
    [[nodiscard]]
    bool operator()(const BTreeEntryKey & left, const BTreeEntryKey & right) const noexcept;
};

/**
 * @brief B+ 树叶子页条目
 */
using BTreeLeafEntry = BTreeEntryKey;

/**
 * @brief B+ 树叶子页
 */
class BTreeLeafPage
{
public:
    explicit BTreeLeafPage(
        BTreePageId page_id,
        BTreePageId previous_page_id = InvalidBTreePageId,
        BTreePageId next_page_id = InvalidBTreePageId
    ) noexcept;

public:
    /**
     * @brief 获取页类型
     */
    [[nodiscard]]
    static constexpr BTreePageType type() noexcept;

    /**
     * @brief 获取页 ID
     */
    [[nodiscard]]
    BTreePageId page_id() const noexcept;

    /**
     * @brief 获取前一叶子页 ID
     */
    [[nodiscard]]
    BTreePageId previous_page_id() const noexcept;

    /**
     * @brief 获取后一叶子页 ID
     */
    [[nodiscard]]
    BTreePageId next_page_id() const noexcept;

    /**
     * @brief 设置前一叶子页 ID
     */
    void set_previous_page_id(BTreePageId page_id) noexcept;

    /**
     * @brief 设置后一叶子页 ID
     */
    void set_next_page_id(BTreePageId page_id) noexcept;

    /**
     * @brief 判断叶子页是否为空
     */
    [[nodiscard]]
    bool empty() const noexcept;

    /**
     * @brief 获取叶子页条目数量
     */
    [[nodiscard]]
    std::size_t size() const noexcept;

    /**
     * @brief 获取叶子页条目
     */
    [[nodiscard]]
    const std::vector<BTreeLeafEntry> & entries() const noexcept;

    /**
     * @brief 查找第一个不小于完整内部键的条目位置
     */
    [[nodiscard]]
    std::size_t lower_bound(const BTreeEntryKey & entry) const noexcept;

    /**
     * @brief 查找第一个标量键不小于给定键的条目位置
     */
    [[nodiscard]]
    std::size_t lower_bound(const ScalarIndexKey & key) const noexcept;

    /**
     * @brief 查找第一个标量键大于给定键的条目位置
     */
    [[nodiscard]]
    std::size_t upper_bound(const ScalarIndexKey & key) const noexcept;

    /**
     * @brief 判断叶子页是否包含给定条目
     */
    [[nodiscard]]
    bool contains(const BTreeEntryKey & entry) const noexcept;

    /**
     * @brief 按序插入叶子条目
     * @return 插入成功返回 true，完整内部键重复时返回 false
     */
    bool insert(BTreeLeafEntry entry);

    /**
     * @brief 删除精确匹配的叶子条目
     * @return 删除成功返回 true，条目不存在时返回 false
     */
    bool erase(const BTreeEntryKey & entry);

private:
    BTreePageId page_id_;                    ///< 页 ID
    BTreePageId previous_page_id_;           ///< 前一叶子页 ID
    BTreePageId next_page_id_;               ///< 后一叶子页 ID
    std::vector<BTreeLeafEntry> entries_;    ///< 有序叶子条目
};

/**
 * @brief B+ 树内部页条目
 * @details separator 是 right_child_id 所指子树中的最小内部键。
 */
struct BTreeInternalEntry
{
    BTreeEntryKey separator;          ///< 右子树最小内部键
    BTreePageId right_child_id;       ///< 右子页 ID
};

/**
 * @brief B+ 树内部页
 * @details first_child_id 处理小于首个 separator 的键；每个条目的右子页处理大于等于对应 separator 的键。
 */
class BTreeInternalPage
{
public:
    BTreeInternalPage(BTreePageId page_id, BTreePageId first_child_id) noexcept;

public:
    /**
     * @brief 获取页类型
     */
    [[nodiscard]]
    static constexpr BTreePageType type() noexcept;

    /**
     * @brief 获取页 ID
     */
    [[nodiscard]]
    BTreePageId page_id() const noexcept;

    /**
     * @brief 获取第一个子页 ID
     */
    [[nodiscard]]
    BTreePageId first_child_id() const noexcept;

    /**
     * @brief 判断内部页是否为空
     */
    [[nodiscard]]
    bool empty() const noexcept;

    /**
     * @brief 获取内部页条目数量
     */
    [[nodiscard]]
    std::size_t size() const noexcept;

    /**
     * @brief 获取子页数量
     */
    [[nodiscard]]
    std::size_t child_count() const noexcept;

    /**
     * @brief 获取内部页条目
     */
    [[nodiscard]]
    const std::vector<BTreeInternalEntry> & entries() const noexcept;

    /**
     * @brief 根据完整内部键选择子页
     */
    [[nodiscard]]
    BTreePageId child_for(const BTreeEntryKey & key) const noexcept;

    /**
     * @brief 获取指定位置的子页 ID
     */
    [[nodiscard]]
    std::optional<BTreePageId> child_at(std::size_t index) const noexcept;

    /**
     * @brief 查找子页位置
     */
    [[nodiscard]]
    std::optional<std::size_t> find_child(BTreePageId child_id) const noexcept;

    /**
     * @brief 在指定左子页后插入分隔键和右子页
     * @return 左子页不存在、子页重复或插入后分隔键无序时返回 false
     */
    bool insert_child_after(
        BTreePageId left_child_id,
        BTreeEntryKey separator,
        BTreePageId right_child_id
    );

    /**
     * @brief 更新指定右子页对应的分隔键
     * @return 右子页不存在或更新后分隔键无序时返回 false
     */
    bool replace_separator(BTreePageId right_child_id, BTreeEntryKey separator);

    /**
     * @brief 删除一个子页及其关联分隔键
     * @return 子页不存在或内部页只剩一个子页时返回 false
     */
    bool erase_child(BTreePageId child_id);

private:
    /**
     * @brief 判断分隔键是否严格有序
     */
    [[nodiscard]]
    bool separators_are_strictly_ordered() const noexcept;

private:
    BTreePageId page_id_;                         ///< 页 ID
    BTreePageId first_child_id_;                  ///< 第一个子页 ID
    std::vector<BTreeInternalEntry> entries_;     ///< 有序分隔键与右子页
};

/**
 * @brief 已解码的 B+ 树逻辑页
 */
using BTreePage = std::variant<BTreeInternalPage, BTreeLeafPage>;

/**
 * @brief 获取页类型
 */
[[nodiscard]]
BTreePageType btree_page_type(const BTreePage & page) noexcept;

/**
 * @brief 获取页 ID
 */
[[nodiscard]]
BTreePageId btree_page_id(const BTreePage & page) noexcept;

} // namespace litedb::core::index::btree_index
