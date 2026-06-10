#pragma once

#include <cstddef>
#include <memory_resource>

namespace litedb::memory
{

/**
 * @brief 基于MemoryPool的PMR内存资源
 */
class MemoryPoolResource final : public std::pmr::memory_resource
{
private:
    void * do_allocate(std::size_t bytes, std::size_t alignment) override;

    void do_deallocate(void * ptr, std::size_t bytes, std::size_t alignment) override;

    bool do_is_equal(const std::pmr::memory_resource & other) const noexcept override;
};

/**
 * @brief 获取全局MemoryPool PMR资源
 * @return 全局MemoryPoolResource实例
 */
[[nodiscard]]
std::pmr::memory_resource * memory_pool_resource() noexcept;

} // namespace litedb::memory
