#pragma once

#include <cstddef>

#include "memory/caches/thread_cache.hpp"

namespace litedb::memory
{

/**
 * @brief 内存池
 */
class MemoryPool
{
public:
    [[nodiscard("MemoryPool::allocate()未使用")]]
    /**
     * @brief 从内存池中分配内存
     * @param bytes 要分配的内存大小
     * @param alignment 对齐值
     */
    static void * allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t));

    /**
     * @brief 将内存释放回内存池
     * @param ptr 要释放的内存地址
     * @param bytes 要释放的内存大小
     * @param alignment 对齐值
     */
    static void deallocate(
        void * ptr,
        std::size_t bytes,
        std::size_t alignment = alignof(std::max_align_t)
    ) noexcept;

    /**
     * @brief 判断内存是否适合内存池
     * @param bytes 要分配的内存大小
     * @param alignment 对齐值
     * @return 是否适合
     */
    [[nodiscard("MemoryPool::is_pool_eligible()未使用")]]
    static bool is_pool_eligible(std::size_t bytes, std::size_t alignment) noexcept;
};

} // namespace litedb::memory
