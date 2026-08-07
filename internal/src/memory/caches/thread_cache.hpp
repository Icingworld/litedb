#pragma once

#include <array>

#include "memory/common/free_list.hpp"
#include "memory/common/common.hpp"

namespace litedb::memory
{

/**
 * @brief 线程缓存
 */
class ThreadCache
{
public:
    ~ThreadCache();

private:
    ThreadCache();

    ThreadCache(const ThreadCache &) = delete;

    ThreadCache & operator=(const ThreadCache &) = delete;

public:
    /**
     * @brief 获取线程缓存实例
     * @return 线程缓存实例
     */
    static ThreadCache & get_instance();

    /**
     * @brief 分配内存
     * @param size 要分配的内存大小
     * @return 分配的内存地址，如果分配失败，则返回nullptr
     */
    [[nodiscard]]
    void * allocate(std::size_t size) noexcept;

    /**
     * @brief 释放内存
     * @param ptr 要释放的内存地址
     * @param size 要释放的内存大小
     */
    void deallocate(void * ptr, std::size_t size) noexcept;

private:
    std::array<FreeList, MAX_ARRAY_SIZE> free_lists_;  // 自由内存列表
};

} // namespace litedb::memory
