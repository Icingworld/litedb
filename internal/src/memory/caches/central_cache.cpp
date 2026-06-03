#include "memory/caches/central_cache.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "memory/caches/page_cache.hpp"
#include "memory/common/common.hpp"
#include "memory/common/size.hpp"

namespace litedb::memory
{

namespace
{

// CentralCache 只处理小对象。每个 size class 用对齐后的块大小向 PageCache 申请足够页数。
std::size_t pages_for_size(std::size_t size) noexcept
{
    return std::max<std::size_t>(1, (size + PAGE_SIZE - 1) / PAGE_SIZE);
}

// SpanNode 只保存页号，访问实际页内存时需要还原成页起始地址。
FreeBlock * span_start(SpanNode * span) noexcept
{
    return reinterpret_cast<FreeBlock *>(static_cast<std::uintptr_t>(span->page_id()) << PAGE_SHIFT);
}

// SpanList 是侵入式链表，脱链后的节点 prev / next 会被置空，可用来判断是否已在中心链表中。
bool is_linked_to_central_list(SpanNode * span) noexcept
{
    return span->prev() != nullptr && span->next() != nullptr;
}

void refill_span_free_list(SpanNode * span, std::size_t block_size) noexcept
{
    FreeList & free_list = span->free_list();
    free_list.clear();

    // 将整个 span 按固定块大小切分，剩余不足一个 block 的尾部空间保留不用。
    const std::size_t bytes = span->page_count() * PAGE_SIZE;
    const std::size_t block_count = bytes / block_size;
    char * current = reinterpret_cast<char *>(span_start(span));

    for (std::size_t i = 0; i < block_count; ++i) {
        free_list.push_front(reinterpret_cast<FreeBlock *>(current));
        current += block_size;
    }

    free_list.set_capacity(block_count);
}

SpanNode * fetch_span(std::size_t block_size)
{
    // 新 span 仍然归 PageCache 的 busy 表管理，CentralCache 只负责 span 内 block 的分配状态。
    SpanNode * span = PageCache::get_instance().allocate_span(pages_for_size(block_size));
    if (span == nullptr) {
        return nullptr;
    }

    refill_span_free_list(span, block_size);
    return span;
}

void append_block(FreeBlock *& head, FreeBlock *& tail, FreeBlock * block) noexcept
{
    // 返回给调用方的是一条以 nullptr 结尾的单链表。
    block->set_next(nullptr);
    if (head == nullptr) {
        head = block;
    } else {
        tail->set_next(block);
    }
    tail = block;
}

} // namespace

CentralCache::CentralCache()
    : span_lists_()
    , mutexes_()
{
}

CentralCache::~CentralCache() = default;

CentralCache & CentralCache::get_instance()
{
    static CentralCache instance;
    return instance;
}

FreeBlock * CentralCache::allocate_range(std::size_t size, std::size_t & count)
{
    if (size == 0 || size > MAX_OBJECT_SIZE || count == 0) {
        count = 0;
        return nullptr;
    }

    const std::size_t block_size = Size::round_up(size);
    const std::size_t index = Size::size_to_index(block_size);
    const std::size_t requested_count = count;
    count = 0;

    // 每个 size class 独立加锁，不同规格的批量申请互不阻塞。
    std::lock_guard<std::mutex> lock(mutexes_[index]);

    FreeBlock * head = nullptr;
    FreeBlock * tail = nullptr;

    while (count < requested_count) {
        if (span_lists_[index].empty()) {
            SpanNode * span = fetch_span(block_size);
            if (span == nullptr) {
                break;
            }
            span_lists_[index].push_front(span);
        }

        SpanNode & span = span_lists_[index].front();
        FreeList & free_list = span.free_list();
        assert(!free_list.empty() && "CentralCache span list should not contain empty spans");

        FreeBlock * block = free_list.front();
        free_list.pop_front();
        span.increment_used();
        append_block(head, tail, block);
        ++count;

        // 没有空闲 block 的 span 不能留在 CentralCache 可分配链表里。
        if (free_list.empty()) {
            span_lists_[index].pop_front();
        }
    }

    return head;
}

void CentralCache::release_range(FreeBlock * block, std::size_t size)
{
    if (block == nullptr || size == 0 || size > MAX_OBJECT_SIZE) {
        return;
    }

    const std::size_t block_size = Size::round_up(size);
    const std::size_t index = Size::size_to_index(block_size);
    std::lock_guard<std::mutex> lock(mutexes_[index]);

    // 调用方传入的是一整批同 size class 的 block，以 nullptr 作为链表结束。
    while (block != nullptr) {
        FreeBlock * next = block->next();
        SpanNode * span = PageCache::get_instance().find_span(block);
        assert(span != nullptr && "CentralCache::release_range() received a block outside PageCache");

        const bool was_empty = span->free_list().empty();
        span->free_list().push_front(block);
        span->decrement_used();

        if (span->used() == 0) {
            // span 全部空闲后不再由 CentralCache 持有，清掉 block 链表元数据后归还 PageCache 做页级合并。
            if (is_linked_to_central_list(span)) {
                span_lists_[index].remove(span);
            }
            span->free_list().clear();
            PageCache::get_instance().release_span(span);
        } else if (was_empty && !is_linked_to_central_list(span)) {
            // 之前被分配光的 span 重新有空闲 block，需要挂回对应 size class 链表。
            span_lists_[index].push_front(span);
        }

        block = next;
    }
}

} // namespace litedb::memory
