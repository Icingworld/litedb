#pragma once

#include <cstddef>
#include <iterator>

namespace litedb::memory
{

/**
 * @brief 空闲内存块
 */
class FreeBlock
{
public:
    FreeBlock() noexcept;

    explicit FreeBlock(FreeBlock * next) noexcept;

    /**
     * @brief 禁止拷贝构造
     * @details 内存块被多个实例持有比较危险
     */
    FreeBlock(const FreeBlock &) = delete;

    /**
     * @brief 禁止拷贝赋值
     * @details 内存块被多个实例持有比较危险
     */
    FreeBlock & operator=(const FreeBlock &) = delete;

    ~FreeBlock() = default;

public:
    /**
     * @brief 获取下一个空闲内存块
     * @return 下一个空闲内存块
     */
    [[nodiscard]] FreeBlock * next() const noexcept;

    /**
     * @brief 设置下一个空闲内存块
     * @param next 下一个空闲内存块
     */
    void set_next(FreeBlock * next) noexcept;

private:
    FreeBlock * next_;  ///< 下一个空闲内存块
};

/**
 * @brief 空闲内存块链表迭代器
 */
class FreeListIterator
{
public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = FreeBlock;
    using difference_type = std::ptrdiff_t;
    using pointer = FreeBlock *;
    using reference = FreeBlock &;

public:
    FreeListIterator();

    explicit FreeListIterator(FreeBlock * block) noexcept;

    ~FreeListIterator() = default;

public:
    /**
     * @brief 解引用迭代器
     * @return 当前空闲内存块
     */
    [[nodiscard]]
    reference operator*() const noexcept;

    /**
     * @brief 访问当前空闲内存块的成员
     * @return 当前空闲内存块
     */
    [[nodiscard]]
    pointer operator->() const noexcept;

    /**
     * @brief 前置递增迭代器
     * @return 递增后的迭代器
     */
    FreeListIterator & operator++() noexcept;

    /**
     * @brief 后置递增迭代器
     * @return 递增前的迭代器
     */
    FreeListIterator operator++(int) noexcept;

    /**
     * @brief 比较两个迭代器是否相等
     * @param other 另一个迭代器
     * @return 如果两个迭代器指向同一个空闲内存块，则返回true；否则返回false
     */
    [[nodiscard]]
    bool operator==(const FreeListIterator & other) const noexcept;

    /**
     * @brief 比较两个迭代器是否不相等
     * @param other 另一个迭代器
     * @return 如果两个迭代器指向不同的空闲内存块，则返回true；否则返回false
     */
    [[nodiscard]]
    bool operator!=(const FreeListIterator & other) const noexcept;

private:
    FreeBlock * current_;  ///< 当前空闲内存块
};

/**
 * @brief 空闲内存块链表
 */
class FreeList
{
public:
    using iterator = FreeListIterator;

public:
    FreeList() noexcept;

    /**
     * @brief 禁止拷贝构造
     * @details 侵入式链表不支持拷贝
     */
    FreeList(const FreeList &) = delete;

    /**
     * @brief 禁止拷贝赋值
     * @details 侵入式链表不支持拷贝
     */
    FreeList & operator=(const FreeList &) = delete;

    ~FreeList() = default;

public:
    /**
     * @brief 获取链表头部的空闲内存块
     * @return 链表头部的空闲内存块，如果链表为空，则返回nullptr
     */
    [[nodiscard]]
    FreeBlock * front() noexcept;

    /**
     * @brief 将一个空闲内存块添加到链表头部
     * @param block 要添加的空闲内存块
     * @warning 不验证是否超过容量，调用者需自行管理
     */
    void push_front(FreeBlock * block) noexcept;

    /**
     * @brief 从链表头部移除一个空闲内存块
     * @warning 不验证链表是否为空，调用者需自行管理
     */
    void pop_front() noexcept;

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
     * @brief 获取空闲内存块总容量
     * @return 空闲内存块总容量
     */
    [[nodiscard]]
    std::size_t capacity() const noexcept;

    /**
     * @brief 获取空闲内存块数量
     * @return 空闲内存块数量
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief 清空链表
     */
    void clear() noexcept;

    /**
     * @brief 设置空闲内存块总容量
     * @param capacity 空闲内存块总容量
     */
    void set_capacity(std::size_t capacity) noexcept;

private:
    FreeBlock head_;        ///< 虚拟头节点
    std::size_t size_;      ///< 空闲内存块数量
    std::size_t capacity_;  ///< 空闲内存块总容量
};

} // namespace litedb::memory
