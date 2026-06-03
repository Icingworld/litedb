#include "memory/caches/page_cache.hpp"

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

void test_invalid_page_counts()
{
    litedb::memory::PageCache & page_cache = litedb::memory::PageCache::get_instance();

    require(page_cache.allocate_span(0) == nullptr, "allocate_span(0) should fail");
    require(
        page_cache.allocate_span(litedb::memory::MAX_PAGE_COUNT + 1) == nullptr,
        "allocate_span(MAX_PAGE_COUNT + 1) should fail"
    );
}

void test_split_reuse_and_full_coalescing()
{
    using namespace litedb::memory;

    PageCache & page_cache = PageCache::get_instance();

    SpanNode * tail_span = page_cache.allocate_span(10);
    require(tail_span != nullptr, "allocating tail span failed");
    require(tail_span->page_count() == 10, "tail span has unexpected page count");

    const std::size_t base_page_id = tail_span->page_id() - (MAX_PAGE_COUNT - tail_span->page_count());

    SpanNode * middle_span = page_cache.allocate_span(20);
    require(middle_span != nullptr, "allocating middle span failed");
    require(middle_span->page_count() == 20, "middle span has unexpected page count");
    require(
        middle_span->page_id() + middle_span->page_count() == tail_span->page_id(),
        "second allocation should be adjacent to the first allocation"
    );

    const std::size_t tail_page_id = tail_span->page_id();

    page_cache.release_span(tail_span);

    SpanNode * reused_tail_span = page_cache.allocate_span(10);
    require(reused_tail_span != nullptr, "reallocating tail span failed");
    require(reused_tail_span->page_id() == tail_page_id, "exact-size free span was not reused");
    require(reused_tail_span->page_count() == 10, "reused tail span has unexpected page count");

    page_cache.release_span(reused_tail_span);
    page_cache.release_span(middle_span);

    SpanNode * whole_span = page_cache.allocate_span(MAX_PAGE_COUNT);
    require(whole_span != nullptr, "allocating coalesced full span failed");
    require(whole_span->page_id() == base_page_id, "coalesced span should start at the original base page");
    require(whole_span->page_count() == MAX_PAGE_COUNT, "coalesced span should contain every page");

    page_cache.release_span(whole_span);
}

} // namespace

int main()
{
    try {
        test_invalid_page_counts();
        test_split_reuse_and_full_coalescing();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
