#pragma once

#include <cstddef>

namespace litedb::memory
{

/**
 * @brief 系统内存分配器
 */
class SystemAllocator
{
public:
    /**
     * @brief 从系统中以对齐的方式分配内存
     * @param bytes 要分配的内存大小
     * @return 分配的内存地址
     */
    [[nodiscard("SystemAllocator::allocate()未使用")]]
    static void * allocate(std::size_t bytes) noexcept;

    /**
     * @brief 将内存释放回系统
     * @param pointer 要释放的内存地址
     * @param bytes 要释放的内存大小
     */
    static void deallocate(void * pointer, std::size_t bytes) noexcept;
};

} // namespace litedb::memory
