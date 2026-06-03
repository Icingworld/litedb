#include "memory/caches/central_cache.hpp"
#include "memory/caches/page_cache.hpp"
#include "memory/common/common.hpp"

#include <iostream>
#include <stdexcept>

namespace
{

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::size_t count_blocks(litedb::memory::FreeBlock * block)
{
    std::size_t count = 0;
    while (block != nullptr) {
        ++count;
        block = block->next();
    }
    return count;
}

void test_invalid_size_requests()
{
    litedb::memory::CentralCache & central_cache = litedb::memory::CentralCache::get_instance();

    std::size_t count = 10;
    require(central_cache.allocate_range(0, count) == nullptr, "allocate_range(0) should fail");
    require(count == 0, "allocate_range(0) should reset count");

    count = 10;
    require(
        central_cache.allocate_range(litedb::memory::MAX_OBJECT_SIZE + 1, count) == nullptr,
        "allocate_range(too large) should fail"
    );
    require(count == 0, "allocate_range(too large) should reset count");
}

void test_small_batch_allocate_release_and_reuse()
{
    litedb::memory::CentralCache & central_cache = litedb::memory::CentralCache::get_instance();

    std::size_t count = 10;
    litedb::memory::FreeBlock * blocks = central_cache.allocate_range(64, count);
    require(blocks != nullptr, "small allocation should succeed");
    require(count == 10, "small allocation should return requested count");
    require(count_blocks(blocks) == 10, "small allocation list length mismatch");

    litedb::memory::FreeBlock * first = blocks;
    central_cache.release_range(blocks, 64);

    count = 1;
    blocks = central_cache.allocate_range(64, count);
    require(blocks != nullptr, "small reallocation should succeed");
    require(count == 1, "small reallocation should return one block");
    require(blocks == first, "small reallocation should reuse a released block");

    central_cache.release_range(blocks, 64);
}

void test_large_cross_page_batch()
{
    litedb::memory::CentralCache & central_cache = litedb::memory::CentralCache::get_instance();

    std::size_t count = 3;
    litedb::memory::FreeBlock * blocks = central_cache.allocate_range(litedb::memory::PAGE_SIZE + 128, count);
    require(blocks != nullptr, "large allocation should succeed");
    require(count == 3, "large allocation should return requested count");
    require(count_blocks(blocks) == 3, "large allocation list length mismatch");

    central_cache.release_range(blocks, litedb::memory::PAGE_SIZE + 128);
}

void test_full_release_returns_span_to_page_cache()
{
    using namespace litedb::memory;

    CentralCache & central_cache = CentralCache::get_instance();

    std::size_t count = 2;
    FreeBlock * blocks = central_cache.allocate_range(MAX_PAGE_COUNT * PAGE_SIZE / 2, count);
    require(blocks != nullptr, "half-max allocation should succeed");
    require(count == 2, "half-max allocation should return two blocks");
    require(count_blocks(blocks) == 2, "half-max allocation list length mismatch");

    central_cache.release_range(blocks, MAX_PAGE_COUNT * PAGE_SIZE / 2);

    SpanNode * whole_span = PageCache::get_instance().allocate_span(MAX_PAGE_COUNT);
    require(whole_span != nullptr, "PageCache should receive the fully released central span");
    require(whole_span->page_count() == MAX_PAGE_COUNT, "returned span should contain every page");

    PageCache::get_instance().release_span(whole_span);
}

} // namespace

int main()
{
    try {
        test_invalid_size_requests();
        test_small_batch_allocate_release_and_reuse();
        test_large_cross_page_batch();
        test_full_release_returns_span_to_page_cache();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
