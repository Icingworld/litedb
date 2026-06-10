#pragma once

#include <cstddef>

namespace litedb::memory
{

/**
 * @brief 内存块大小类
 */
class Size
{
public:
    /**
     * @brief 将索引转换为内存块大小
     * @param index 索引
     * @return 内存块大小
     */
    [[nodiscard]]
    static std::size_t index_to_size(std::size_t index) noexcept;

    /**
     * @brief 将内存块大小转换为索引
     * @param size 对齐后的内存块大小
     * @return 索引
     */
    [[nodiscard]]
    static std::size_t size_to_index(std::size_t size) noexcept;

    /**
     * @brief 将内存块大小向上对齐到最接近的对齐值
     * @param size 内存块大小
     * @return 对齐后的内存块大小
    */
    [[nodiscard]]
    static std::size_t round_up(std::size_t size) noexcept;
};

} // namespace litedb::memory
