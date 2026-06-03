#include "memory/caches/thread_cache.hpp"

#include <algorithm>
#include <cstddef>

#include "memory/caches/central_cache.hpp"
#include "memory/common/common.hpp"
#include "memory/common/size.hpp"

namespace litedb::memory
{

namespace
{

constexpr std::size_t MAX_BATCH_COUNT = 64;

// 限制每次补货的对象数量，同时让单批搬运量不超过最大小对象范围。
std::size_t max_batch(std::size_t block_size) noexcept
{
    return std::min<std::size_t>(MAX_BATCH_COUNT, std::max<std::size_t>(1, MAX_OBJECT_SIZE / block_size));
}

// 将以 nullptr 结尾的内存块链表移动到本地自由链表。
void append_to_free_list(FreeList & free_list, FreeBlock * block) noexcept
{
    while (block != nullptr) {
        FreeBlock * next = block->next();
        free_list.push_front(block);
        block = next;
    }
}

// 从本地自由链表摘取最多 count 个内存块，并组成以 nullptr 结尾的链表。
FreeBlock * take_range(FreeList & free_list, std::size_t count) noexcept
{
    FreeBlock * head = nullptr;
    FreeBlock * tail = nullptr;

    while (count > 0 && !free_list.empty()) {
        FreeBlock * block = free_list.front();
        free_list.pop_front();
        block->set_next(nullptr);

        if (head == nullptr) {
            head = block;
        } else {
            tail->set_next(block);
        }
        tail = block;
        --count;
    }

    return head;
}

} // namespace

ThreadCache::ThreadCache()
    : free_lists_()
{
    for (FreeList & free_list : free_lists_) {
        free_list.set_capacity(1);
    }
}

ThreadCache::~ThreadCache()
{
    // 线程退出前必须归还本地缓存的内存块，否则 PageCache 无法感知完整空闲的 span 并进行合并。
    for (std::size_t index = 0; index < free_lists_.size(); ++index) {
        FreeList & free_list = free_lists_[index];
        if (free_list.empty()) {
            continue;
        }

        FreeBlock * blocks = take_range(free_list, free_list.size());
        CentralCache::get_instance().release_range(blocks, Size::index_to_size(index));
        free_list.clear();
    }
}

ThreadCache & ThreadCache::get_instance()
{
    static thread_local ThreadCache instance;
    return instance;
}

void * ThreadCache::allocate(std::size_t size) noexcept
{
    if (size == 0 || size > MAX_OBJECT_SIZE) {
        return nullptr;
    }

    const std::size_t block_size = Size::round_up(size);
    const std::size_t index = Size::size_to_index(block_size);
    FreeList & free_list = free_lists_[index];

    if (!free_list.empty()) {
        // 快路径：当前线程已有缓存块时，不需要跨线程同步。
        FreeBlock * block = free_list.front();
        free_list.pop_front();
        return block;
    }

    // 本地未命中时，从 CentralCache 批量拉取；返回其中一个，其余留在线程本地。
    std::size_t count = free_list.capacity();
    FreeBlock * blocks = CentralCache::get_instance().allocate_range(block_size, count);
    if (blocks == nullptr || count == 0) {
        return nullptr;
    }

    FreeBlock * result = blocks;
    blocks = blocks->next();
    result->set_next(nullptr);
    append_to_free_list(free_list, blocks);

    // 慢启动每次补货后增长 1，直到达到该 size class 的批量上限。
    if (free_list.capacity() < max_batch(block_size)) {
        free_list.set_capacity(free_list.capacity() + 1);
    }

    return result;
}

void ThreadCache::deallocate(void * ptr, std::size_t size) noexcept
{
    if (ptr == nullptr || size == 0 || size > MAX_OBJECT_SIZE) {
        return;
    }

    const std::size_t block_size = Size::round_up(size);
    const std::size_t index = Size::size_to_index(block_size);
    FreeList & free_list = free_lists_[index];

    free_list.push_front(reinterpret_cast<FreeBlock *>(ptr));

    if (free_list.size() <= free_list.capacity() * 2) {
        return;
    }

    // 控制本地缓存上界：空闲块过多时，归还一批给 CentralCache。
    FreeBlock * blocks = take_range(free_list, free_list.capacity());
    CentralCache::get_instance().release_range(blocks, block_size);
}

} // namespace litedb::memory
