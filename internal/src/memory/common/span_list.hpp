#pragma once

#include <cstddef>
#include <iterator>

#include "memory/common/free_list.hpp"

namespace litedb::memory
{

/**
 * @brief 页段节点
 */
class SpanNode
{
public:
    SpanNode() noexcept;

    SpanNode(const SpanNode &) = delete;

    SpanNode & operator=(const SpanNode &) = delete;

    ~SpanNode() = default;

public:
    /**
     * @brief 获取上一个页段节点
     * @return 上一个页段节点，如果不存在则返回nullptr
     */
    [[nodiscard]]
    SpanNode * prev() const noexcept;

    /**
     * @brief 设置上一个页段节点
     * @param prev 上一个页段节点
     */
    void set_prev(SpanNode * prev) noexcept;

    /**
     * @brief 获取下一个页段节点
     * @return 下一个页段节点，如果不存在则返回nullptr
     */
    [[nodiscard]]
    SpanNode * next() const noexcept;

    /**
     * @brief 设置下一个页段节点
     * @param next 下一个页段节点
     */
    void set_next(SpanNode * next) noexcept;

    /**
     * @brief 获取页段内的空闲内存块链表
     * @return 空闲内存块链表
     */
    [[nodiscard]]
    FreeList & free_list() noexcept;

    /**
     * @brief 获取页段内的空闲内存块链表
     * @return 空闲内存块链表
     */
    [[nodiscard]]
    const FreeList & free_list() const noexcept;

    /**
     * @brief 获取页段逻辑编号
     * @return 页段逻辑编号
     */
    [[nodiscard]]
    std::size_t page_id() const noexcept;

    /**
     * @brief 设置页段逻辑编号
     * @param page_id 页段逻辑编号
     */
    void set_page_id(std::size_t page_id) noexcept;

    /**
     * @brief 获取页段包含的页数
     * @return 页段包含的页数
     */
    [[nodiscard]]
    std::size_t page_count() const noexcept;

    /**
     * @brief 设置页段包含的页数
     * @param page_count 页段包含的页数
     */
    void set_page_count(std::size_t page_count) noexcept;

    /**
     * @brief 获取已使用的内存块数量
     * @return 已使用的内存块数量
     */
    [[nodiscard]]
    std::size_t used() const noexcept;

    /**
     * @brief 递增已使用的内存块数量
     */
    void increment_used() noexcept;

    /**
     * @brief 递减已使用的内存块数量
     * @warning 不验证是否大于0，调用者需自行管理
     */
    void decrement_used() noexcept;

    /**
     * @brief 将内存地址转换为页段逻辑编号
     * @param pointer 内存地址
     * @return 页段逻辑编号
     */
    [[nodiscard]]
    static std::size_t ptr_to_id(void * pointer) noexcept;

private:
    SpanNode * prev_;  // 上一个节点
    SpanNode * next_;  // 下一个节点

    FreeList free_list_;  // 空闲内存块链表
    std::size_t page_id_;  // 页段逻辑编号
    std::size_t page_count_;  // 页段包含的页数
    std::size_t used_;  // 已使用的内存块数量
};

/**
 * @brief 页段链表迭代器
 */
class SpanListIterator
{
public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = SpanNode;
    using difference_type = std::ptrdiff_t;
    using pointer = SpanNode *;
    using reference = SpanNode &;

public:
    SpanListIterator();

    explicit SpanListIterator(SpanNode * node) noexcept;

    ~SpanListIterator() = default;

public:
    /**
     * @brief 解引用迭代器
     * @return 当前页段节点
     */
    [[nodiscard]]
    reference operator*() const noexcept;

    /**
     * @brief 访问当前页段节点的成员
     * @return 当前页段节点
     */
    [[nodiscard]]
    pointer operator->() const noexcept;

    /**
     * @brief 前置递增迭代器
     * @return 递增后的迭代器
     */
    SpanListIterator & operator++() noexcept;

    /**
     * @brief 后置递增迭代器
     * @return 递增前的迭代器
     */
    SpanListIterator operator++(int) noexcept;

    /**
     * @brief 比较两个迭代器是否相等
     * @param other 另一个迭代器
     * @return 如果两个迭代器指向同一个页段节点，则返回true；否则返回false
     */
    [[nodiscard]]
    bool operator==(const SpanListIterator & other) const noexcept;

    /**
     * @brief 比较两个迭代器是否不相等
     * @param other 另一个迭代器
     * @return 如果两个迭代器指向不同的页段节点，则返回true；否则返回false
     */
    [[nodiscard]]
    bool operator!=(const SpanListIterator & other) const noexcept;

private:
    SpanNode * current_;  // 当前页段节点
};

/**
 * @brief 页段链表
 */
class SpanList
{
public:
    using iterator = SpanListIterator;

public:
    SpanList() noexcept;

    /**
     * @brief 禁止拷贝构造
     * @details 侵入式链表不支持拷贝
     */
    SpanList(const SpanList &) = delete;

    /**
     * @brief 禁止拷贝赋值
     * @details 侵入式链表不支持拷贝
     */
    SpanList & operator=(const SpanList &) = delete;

    ~SpanList() = default;

public:
    /**
     * @brief 获取链表头部的页段节点
     * @return 链表头部的页段节点
     * @warning 不验证链表是否为空，调用者需自行管理
     */
    [[nodiscard]]
    SpanNode & front() noexcept;

    /**
     * @brief 获取链表尾部的页段节点
     * @return 链表尾部的页段节点
     * @warning 不验证链表是否为空，调用者需自行管理
     */
    [[nodiscard]]
    SpanNode & back() noexcept;

    /**
     * @brief 将一个页段节点添加到链表头部
     * @param node 要添加的页段节点
     */
    void push_front(SpanNode * node) noexcept;

    /**
     * @brief 将一个页段节点添加到链表尾部
     * @param node 要添加的页段节点
     */
    void push_back(SpanNode * node) noexcept;

    /**
     * @brief 从链表头部移除一个页段节点
     * @warning 不验证链表是否为空，调用者需自行管理
     */
    void pop_front() noexcept;

    /**
     * @brief 从链表尾部移除一个页段节点
     * @warning 不验证链表是否为空，调用者需自行管理
     */
    void pop_back() noexcept;

    /**
     * @brief 从链表中移除指定页段节点
     * @param node 要移除的页段节点
     * @warning 不验证节点是否在链表中，调用者需自行管理
     */
    void remove(SpanNode * node) noexcept;

    /**
     * @brief 获取链表起始的迭代器
     * @return 链表起始的迭代器，如果链表为空，则返回end()迭代器
     */
    [[nodiscard]]
    iterator begin() noexcept;

    /**
     * @brief 获取链表末尾的迭代器
     * @return 链表末尾的迭代器
     */
    [[nodiscard]]
    iterator end() noexcept;

    /**
     * @brief 检查链表是否为空
     * @return 如果链表为空，则返回true；否则返回false
     */
    [[nodiscard]]
    bool empty() const noexcept;

    /**
     * @brief 获取页段节点数量
     * @return 页段节点数量
     */
    [[nodiscard]]
    std::size_t size() const noexcept;

    /**
     * @brief 清空链表
     */
    void clear() noexcept;

private:
    SpanNode head_;     // 虚拟头节点
    std::size_t size_;  // 页段节点数量
};

} // namespace litedb::memory
