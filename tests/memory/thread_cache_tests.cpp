#include "memory/caches/thread_cache.hpp"
#include "memory/common/common.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{

constexpr std::array<std::size_t, 4> CONCURRENT_THREAD_CACHE_SIZES {
    32,
    64,
    128,
    litedb::memory::PAGE_SIZE + 128,
};

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_invalid_size_requests()
{
    litedb::memory::ThreadCache & thread_cache = litedb::memory::ThreadCache::get_instance();

    require(thread_cache.allocate(0) == nullptr, "allocate(0) should fail");
    require(thread_cache.allocate(litedb::memory::MAX_OBJECT_SIZE + 1) == nullptr, "allocate(too large) should fail");

    thread_cache.deallocate(nullptr, 64);
    thread_cache.deallocate(nullptr, 0);
}

void test_small_allocate_release_and_reuse()
{
    litedb::memory::ThreadCache & thread_cache = litedb::memory::ThreadCache::get_instance();

    void * first = thread_cache.allocate(64);
    require(first != nullptr, "small allocation should succeed");

    thread_cache.deallocate(first, 64);

    void * second = thread_cache.allocate(64);
    require(second == first, "small allocation should reuse local thread cache block");

    thread_cache.deallocate(second, 64);
}

void test_multiple_small_allocations_refill_from_central()
{
    litedb::memory::ThreadCache & thread_cache = litedb::memory::ThreadCache::get_instance();

    std::array<void *, 32> blocks {};
    for (void *& block : blocks) {
        block = thread_cache.allocate(128);
        require(block != nullptr, "repeated small allocation should succeed");
    }

    for (void * block : blocks) {
        thread_cache.deallocate(block, 128);
    }
}

void test_release_over_threshold_and_reallocate()
{
    litedb::memory::ThreadCache & thread_cache = litedb::memory::ThreadCache::get_instance();

    std::array<void *, 96> blocks {};
    for (void *& block : blocks) {
        block = thread_cache.allocate(32);
        require(block != nullptr, "bulk allocation should succeed");
    }

    for (void * block : blocks) {
        thread_cache.deallocate(block, 32);
    }

    void * block = thread_cache.allocate(32);
    require(block != nullptr, "allocation after threshold release should succeed");
    thread_cache.deallocate(block, 32);
}

void test_cross_page_object()
{
    litedb::memory::ThreadCache & thread_cache = litedb::memory::ThreadCache::get_instance();

    void * block = thread_cache.allocate(litedb::memory::PAGE_SIZE + 128);
    require(block != nullptr, "cross-page allocation should succeed");

    thread_cache.deallocate(block, litedb::memory::PAGE_SIZE + 128);
}

void test_concurrent_thread_cache_allocations()
{
    constexpr std::size_t THREAD_COUNT = 8;
    constexpr std::size_t ITERATIONS = 128;

    std::array<std::exception_ptr, THREAD_COUNT> exceptions {};
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);

    for (std::size_t thread_index = 0; thread_index < THREAD_COUNT; ++thread_index) {
        threads.emplace_back([thread_index, &exceptions]() {
            try {
                litedb::memory::ThreadCache & thread_cache = litedb::memory::ThreadCache::get_instance();
                std::vector<std::pair<void *, std::size_t>> blocks;
                blocks.reserve(ITERATIONS);

                for (std::size_t i = 0; i < ITERATIONS; ++i) {
                    const std::size_t size =
                        CONCURRENT_THREAD_CACHE_SIZES[(i + thread_index) % CONCURRENT_THREAD_CACHE_SIZES.size()];
                    void * block = thread_cache.allocate(size);
                    require(block != nullptr, "concurrent ThreadCache allocation should succeed");
                    blocks.emplace_back(block, size);
                }

                for (auto [block, size] : blocks) {
                    thread_cache.deallocate(block, size);
                }
            } catch (...) {
                exceptions[thread_index] = std::current_exception();
            }
        });
    }

    for (std::thread & thread : threads) {
        thread.join();
    }

    for (const std::exception_ptr & exception : exceptions) {
        if (exception != nullptr) {
            std::rethrow_exception(exception);
        }
    }
}

} // namespace

int main()
{
    try {
        test_invalid_size_requests();
        test_small_allocate_release_and_reuse();
        test_multiple_small_allocations_refill_from_central();
        test_release_over_threshold_and_reallocate();
        test_cross_page_object();
        test_concurrent_thread_cache_allocations();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
