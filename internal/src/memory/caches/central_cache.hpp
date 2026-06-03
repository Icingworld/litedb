#pragma once

#include <mutex>
#include <array>

#include "memory/common/free_list.hpp"
#include "memory/common/span_list.hpp"
#include "memory/common/common.hpp"

namespace litedb::memory
{

/**
 * @brief 中心缓存
 */
class CentralCache
{
public:
    ~CentralCache();

private:
    CentralCache();

    CentralCache(const CentralCache &) = delete;

    CentralCache & operator=(const CentralCache &) = delete;

public:
    /**
     * @brief 获取中心缓存实例
     * @return 中心缓存实例
     */
    static CentralCache & get_instance();

    /**
     * @brief 获取指定大小的空闲内存块
     * @param size 内存块大小
     * @param count 分配的内存块数量
     * @return 分配的内存块
     */
    [[nodiscard("CentralCache::allocate_range()未使用")]]
    FreeBlock * allocate_range(std::size_t size, std::size_t & count);

    /**
     * @brief 释放内存块
     * @param block 要释放的内存块
     * @param size 内存块大小
     */
    void release_range(FreeBlock * block, std::size_t size);

private:
    std::array<SpanList, MAX_ARRAY_SIZE> span_lists_;  // 页段链表数组
    std::array<std::mutex, MAX_ARRAY_SIZE> mutexes_;   // 每个页段链表对应的互斥锁数组
};

} // namespace litedb::memory
