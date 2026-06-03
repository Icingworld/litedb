#include "memory/caches/page_cache.hpp"

#include <cassert>
#include <algorithm>

#include "memory/common/platform.hpp"

namespace litedb::memory
{

namespace
{

/**
 * @brief 将页段映射到繁忙页段映射表
 * @param busy_span_map 繁忙页段映射表
 * @param span 要映射的页段节点
 */
void map_span_to_busy(std::map<std::size_t, SpanNode *> & busy_span_map, SpanNode * span)
{
    busy_span_map[span->page_id()] = span;
    busy_span_map[span->page_id() + span->page_count() - 1] = span;
}

/**
 * @brief 将页段从空闲页段映射表中移除
 * @param free_span_map 空闲页段映射表
 * @param span 要移除的页段节点
 */
void unmap_span_from_free(std::unordered_map<std::size_t, SpanNode *> & free_span_map, SpanNode * span)
{
    free_span_map.erase(span->page_id());
    free_span_map.erase(span->page_id() + span->page_count() - 1);
}

/**
 * @brief 将页段映射到空闲页段映射表
 * @param free_span_map 空闲页段映射表
 * @param span 要映射的页段节点
 */
void map_span_to_free(std::unordered_map<std::size_t, SpanNode *> & free_span_map, SpanNode * span)
{
    free_span_map[span->page_id()] = span;
    free_span_map[span->page_id() + span->page_count() - 1] = span;
}

} // namespace

PageCache::PageCache()
    : mutex_()
    , span_lists_()
    , free_span_map_()
    , busy_span_map_()
    , align_pointers_()
{
}

PageCache::~PageCache()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 在线程缓存和中心缓存析构时，所有内存应当已经全部回归页缓存，因此所有页段节点都应当在空闲页段链表中
    assert(busy_span_map_.empty() && "PageCache析构时繁忙页段映射表不应当有任何页段");

    // 销毁所有空闲页段节点
    std::ranges::for_each(span_lists_, [](SpanList & span_list) {
        while (!span_list.empty()) {
            SpanNode & span = span_list.front();
            span_list.pop_front();
            delete &span;
        }
    });

    std::ranges::for_each(align_pointers_, [this](void * pointer) {
        // 释放原始内存
        SystemAllocator::deallocate(pointer, MAX_PAGE_COUNT * PAGE_SIZE);
    });
}

PageCache & PageCache::get_instance()
{
    static PageCache instance;
    return instance;
}

SpanNode * PageCache::allocate_span(std::size_t page_count)
{
    if (page_count == 0 || page_count > MAX_PAGE_COUNT) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 如果有空闲页段，直接从链表中获取
    if (!span_lists_[page_count - 1].empty()) {
        SpanNode & span = span_lists_[page_count - 1].front();
        span_lists_[page_count - 1].pop_front();

        // 从空闲页段映射表中移除，并添加到繁忙页段映射表中
        unmap_span_from_free(free_span_map_, &span);
        map_span_to_busy(busy_span_map_, &span);

        return &span;
    }

    // 没有正好这么大的页段，尝试从更大块内存中切出页段
    for (std::size_t index = page_count; index < MAX_PAGE_COUNT; ++index) {
        if (span_lists_[index].empty()) {
            continue;
        }

        // 取出一个更大的页段，该页段的页数为 index + 1
        SpanNode & bigger_span = span_lists_[index].front();
        span_lists_[index].pop_front();

        // 将该页段切分为两部分，前面页段页数为 index + 1 - page_count，后面页段页数为 page_count
        // 新建一个页段储存页数为 page_count 的页段
        SpanNode * split_span = new SpanNode();
        split_span->set_page_id(bigger_span.page_id() + index + 1 - page_count);
        split_span->set_page_count(page_count);

        // 更新原有大页段的页数为 index + 1 - page_count
        bigger_span.set_page_count(index + 1 - page_count);

        span_lists_[bigger_span.page_count() - 1].push_front(&bigger_span);
        // bigger_span 的起始页号不变，只需要更新末尾页号
        free_span_map_.erase(bigger_span.page_id() + bigger_span.page_count() + split_span->page_count() - 1);
        free_span_map_[bigger_span.page_id() + bigger_span.page_count() - 1] = &bigger_span;

        // 将切分出的页段映射到繁忙页段映射表
        map_span_to_busy(busy_span_map_, split_span);

        return split_span;
    }

    // 没找到更大的页段，向系统申请一个最大的页段
    void * pointer = SystemAllocator::allocate(MAX_PAGE_COUNT * PAGE_SIZE);
    if (pointer == nullptr) {
        return nullptr;
    }

    align_pointers_.push_back(pointer);

    // 新建一个页段储存页数为 MAX_PAGE_COUNT 的页段
    SpanNode * max_span = new SpanNode();

    if (page_count == MAX_PAGE_COUNT) [[unlikely]] {
        max_span->set_page_id(SpanNode::ptr_to_id(pointer));
        max_span->set_page_count(MAX_PAGE_COUNT);

        map_span_to_busy(busy_span_map_, max_span);

        return max_span;
    } [[likely]]

    max_span->set_page_id(SpanNode::ptr_to_id(pointer));
    max_span->set_page_count(MAX_PAGE_COUNT - page_count);

    // 新建一个页段储存页数为 page_count 的页段
    SpanNode * split_span = new SpanNode();
    split_span->set_page_id(max_span->page_id() + MAX_PAGE_COUNT - page_count);
    split_span->set_page_count(page_count);

    // 将两个页段添加到相应的链表和映射表中
    span_lists_[max_span->page_count() - 1].push_front(max_span);
    map_span_to_free(free_span_map_, max_span);
    map_span_to_busy(busy_span_map_, split_span);

    return split_span;
}

void PageCache::release_span(SpanNode * span)
{
    assert(span != nullptr && "PageCache::release_span()不能释放空指针");
    // 不验证 span 是否在繁忙页段映射表中，假设调用者能够传入正确的页段

    std::lock_guard<std::mutex> lock(mutex_);

    // 从繁忙页段映射表中移除
    busy_span_map_.erase(span->page_id());
    busy_span_map_.erase(span->page_id() + span->page_count() - 1);

    // 向前寻找空闲的页段
    auto prev_it = free_span_map_.find(span->page_id() - 1);
    while (prev_it != free_span_map_.end()) {
        SpanNode * prev_span = prev_it->second;

        // 如果合并后的页段超过最大页数，则停止合并
        if (span->page_count() + prev_span->page_count() > MAX_PAGE_COUNT) {
            break;
        }

        // 从空闲页段链表和映射表中移除被合并的页段
        span_lists_[prev_span->page_count() - 1].remove(prev_span);
        unmap_span_from_free(free_span_map_, prev_span);

        // 合并页段
        span->set_page_id(prev_span->page_id());
        span->set_page_count(prev_span->page_count() + span->page_count());

        // 销毁被合并的页段节点
        delete prev_span;

        // 继续向前寻找空闲的页段
        prev_it = free_span_map_.find(span->page_id() - 1);
    }

    // 向后寻找空闲的页段
    auto next_it = free_span_map_.find(span->page_id() + span->page_count());
    while (next_it != free_span_map_.end()) {
        SpanNode * next_span = next_it->second;

        // 如果合并后的页段超过最大页数，则停止合并
        if (span->page_count() + next_span->page_count() > MAX_PAGE_COUNT) {
            break;
        }

        // 从空闲页段链表和映射表中移除被合并的页段
        span_lists_[next_span->page_count() - 1].remove(next_span);
        unmap_span_from_free(free_span_map_, next_span);

        // 合并页段
        span->set_page_count(next_span->page_count() + span->page_count());

        delete next_span;

        // 继续向后寻找空闲的页段
        next_it = free_span_map_.find(span->page_id() + span->page_count());
    }

    // 将合并后的页段添加到空闲页段链表和映射表中
    map_span_to_free(free_span_map_, span);
    span_lists_[span->page_count() - 1].push_front(span);
}

SpanNode * PageCache::find_span(void * pointer)
{
    if (pointer == nullptr) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const std::size_t page_id = SpanNode::ptr_to_id(pointer);
    const auto it = busy_span_map_.lower_bound(page_id);
    if (it == busy_span_map_.end()) {
        return nullptr;
    }

    SpanNode * span = it->second;
    const std::size_t span_begin = span->page_id();
    const std::size_t span_end = span_begin + span->page_count();
    if (page_id < span_begin || page_id >= span_end) {
        return nullptr;
    }

    return span;
}

} // namespace litedb::memory
