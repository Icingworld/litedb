#include "memory/memory_pool.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{

struct Request
{
    std::size_t bytes;
    std::size_t alignment;
};

constexpr std::array<Request, 5> CONCURRENT_MEMORY_POOL_REQUESTS {{
    {32, alignof(std::max_align_t)},
    {128, alignof(std::max_align_t)},
    {litedb::memory::PAGE_SIZE + 128, alignof(std::max_align_t)},
    {129, 64},
    {128, 8192},
}};

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool is_aligned(void * ptr, std::size_t alignment)
{
    return reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0;
}

void test_invalid_request()
{
    require(litedb::memory::MemoryPool::allocate(0) == nullptr, "allocate(0) should fail");
    require(!litedb::memory::MemoryPool::is_pool_eligible(0, alignof(std::max_align_t)), "zero bytes is not pool eligible");
}

void test_small_object()
{
    void * ptr = litedb::memory::MemoryPool::allocate(64);
    require(ptr != nullptr, "small allocation should succeed");
    litedb::memory::MemoryPool::deallocate(ptr, 64);

    void * again = litedb::memory::MemoryPool::allocate(64);
    require(again != nullptr, "small reallocation should succeed");
    litedb::memory::MemoryPool::deallocate(again, 64);
}

void test_large_object_system_path()
{
    void * ptr = litedb::memory::MemoryPool::allocate(litedb::memory::MAX_OBJECT_SIZE + 1);
    require(ptr != nullptr, "large allocation should use system path");
    require(
        !litedb::memory::MemoryPool::is_pool_eligible(
            litedb::memory::MAX_OBJECT_SIZE + 1,
            alignof(std::max_align_t)
        ),
        "large allocation is not pool eligible"
    );
    litedb::memory::MemoryPool::deallocate(ptr, litedb::memory::MAX_OBJECT_SIZE + 1);
}

void test_high_alignment_system_path()
{
    void * ptr = litedb::memory::MemoryPool::allocate(128, 8192);
    require(ptr != nullptr, "high-alignment allocation should succeed");
    require(is_aligned(ptr, 8192), "high-alignment allocation is misaligned");
    require(!litedb::memory::MemoryPool::is_pool_eligible(128, 8192), "high-alignment allocation should not use pool");
    litedb::memory::MemoryPool::deallocate(ptr, 128, 8192);
}

void test_block_size_alignment_fallback()
{
    void * ptr = litedb::memory::MemoryPool::allocate(129, 64);
    require(ptr != nullptr, "fallback alignment allocation should succeed");
    require(is_aligned(ptr, 64), "fallback allocation is misaligned");
    require(!litedb::memory::MemoryPool::is_pool_eligible(129, 64), "misaligned size class should not use pool");
    litedb::memory::MemoryPool::deallocate(ptr, 129, 64);
}

void test_concurrent_memory_pool_allocations()
{
    constexpr std::size_t THREAD_COUNT = 8;
    constexpr std::size_t ITERATIONS = 96;

    std::array<std::exception_ptr, THREAD_COUNT> exceptions {};
    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);

    for (std::size_t thread_index = 0; thread_index < THREAD_COUNT; ++thread_index) {
        threads.emplace_back([thread_index, &exceptions]() {
            try {
                std::vector<std::pair<void *, Request>> blocks;
                blocks.reserve(ITERATIONS);

                for (std::size_t i = 0; i < ITERATIONS; ++i) {
                    const Request request =
                        CONCURRENT_MEMORY_POOL_REQUESTS[(i + thread_index) % CONCURRENT_MEMORY_POOL_REQUESTS.size()];
                    void * ptr = litedb::memory::MemoryPool::allocate(request.bytes, request.alignment);
                    require(ptr != nullptr, "concurrent MemoryPool allocation should succeed");
                    require(is_aligned(ptr, request.alignment), "concurrent MemoryPool allocation is misaligned");
                    blocks.emplace_back(ptr, request);
                }

                for (auto [ptr, request] : blocks) {
                    litedb::memory::MemoryPool::deallocate(ptr, request.bytes, request.alignment);
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
        test_invalid_request();
        test_small_object();
        test_large_object_system_path();
        test_high_alignment_system_path();
        test_block_size_alignment_fallback();
        test_concurrent_memory_pool_allocations();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
