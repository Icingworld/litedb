#pragma once

#include <mutex>
#include <array>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstddef>

#include "memory/common/span_list.hpp"
#include "memory/common/common.hpp"

namespace litedb::memory
{

/**
 * @brief 页缓存
 */
class PageCache
{
public:
    ~PageCache();

private:
    PageCache();

    PageCache(const PageCache &) = delete;

    PageCache & operator=(const PageCache &) = delete;

public:
    /**
     * @brief 获取页缓存实例
     * @return 页缓存实例
     */
    static PageCache & get_instance();

    /**
     * @brief 分配页段
     * @param page_count 页数
     * @return 分配的页段节点
     */
    [[nodiscard("PageCache::allocate_span()未使用")]]
    SpanNode * allocate_span(std::size_t page_count);

    /**
     * @brief 释放页段
     * @param span 要释放的页段节点
     */
    void release_span(SpanNode * span);

    /**
     * @brief 根据内存地址查找所属的繁忙页段
     * @param pointer 内存地址
     * @return 所属页段，如果地址不属于任何繁忙页段则返回nullptr
     */
    [[nodiscard("PageCache::find_span()未使用")]]
    SpanNode * find_span(void * pointer);

private:
    std::mutex mutex_;
    std::array<SpanList, MAX_PAGE_COUNT> span_lists_;            // 页数到空闲页段链表的映射
    std::unordered_map<std::size_t, SpanNode *> free_span_map_;  // 页号到空闲页段节点的映射
    std::map<std::size_t, SpanNode *> busy_span_map_;            // 页号到繁忙页段节点的映射
    std::vector<void *> align_pointers_;                         // 原始内存地址列表
};

} // namespace litedb::memory
