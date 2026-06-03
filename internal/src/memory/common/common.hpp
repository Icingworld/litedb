#pragma once

#include <cstddef>

namespace litedb::memory
{

constexpr std::size_t PAGE_SIZE = 4096;  // 页大小

constexpr std::size_t PAGE_SHIFT = 12;  // 页位移，4096 = 1 << 12

constexpr std::size_t MAX_PAGE_COUNT = 128;  // 最大页数

constexpr std::size_t MAX_OBJECT_SIZE = 262144;  // 最大对象大小

constexpr std::size_t MAX_ARRAY_SIZE = 208;  // 最大数组大小

} // namespace litedb::memory
