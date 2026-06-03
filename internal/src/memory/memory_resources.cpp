#include "memory/memory_resources.hpp"

#include <new>

#include "memory/memory_pool.hpp"

namespace litedb::memory
{

void * MemoryPoolResource::do_allocate(std::size_t bytes, std::size_t alignment)
{
    void * ptr = MemoryPool::allocate(bytes, alignment);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }

    return ptr;
}

void MemoryPoolResource::do_deallocate(void * ptr, std::size_t bytes, std::size_t alignment)
{
    MemoryPool::deallocate(ptr, bytes, alignment);
}

bool MemoryPoolResource::do_is_equal(const std::pmr::memory_resource & other) const noexcept
{
    return this == &other;
}

std::pmr::memory_resource * memory_pool_resource() noexcept
{
    static MemoryPoolResource resource;
    return &resource;
}

} // namespace litedb::memory
