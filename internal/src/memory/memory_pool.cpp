#include "memory/memory_pool.hpp"

#include <new>

#include "memory/common/common.hpp"
#include "memory/common/size.hpp"

namespace litedb::memory
{

namespace
{

bool is_power_of_two(std::size_t value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

} // namespace

void * MemoryPool::allocate(std::size_t bytes, std::size_t alignment)
{
    if (bytes == 0 || !is_power_of_two(alignment)) {
        return nullptr;
    }

    if (is_pool_eligible(bytes, alignment)) {
        return ThreadCache::get_instance().allocate(bytes);
    }

    return ::operator new(bytes, std::align_val_t(alignment));
}

void MemoryPool::deallocate(void * ptr, std::size_t bytes, std::size_t alignment) noexcept
{
    if (ptr == nullptr || bytes == 0 || !is_power_of_two(alignment)) {
        return;
    }

    if (is_pool_eligible(bytes, alignment)) {
        ThreadCache::get_instance().deallocate(ptr, bytes);
        return;
    }

    ::operator delete(ptr, std::align_val_t(alignment));
}

bool MemoryPool::is_pool_eligible(std::size_t bytes, std::size_t alignment) noexcept
{
    if (bytes == 0 || bytes > MAX_OBJECT_SIZE || !is_power_of_two(alignment)) {
        return false;
    }

    const std::size_t block_size = Size::round_up(bytes);
    return alignment <= PAGE_SIZE && block_size % alignment == 0;
}

} // namespace litedb::memory
