#include "memory/memory_resources.hpp"

#include <cstdint>
#include <iostream>
#include <memory_resource>
#include <stdexcept>
#include <vector>

namespace
{

struct alignas(8192) OverAligned
{
    int value;
};

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

void test_pmr_vector()
{
    std::pmr::vector<int> values(litedb::memory::memory_pool_resource());
    for (int i = 0; i < 128; ++i) {
        values.push_back(i);
    }

    require(values.size() == 128, "pmr vector should store pushed values");
    require(values[42] == 42, "pmr vector should preserve values");
}

void test_over_aligned_vector()
{
    std::pmr::vector<OverAligned> values(litedb::memory::memory_pool_resource());
    values.resize(2);

    require(is_aligned(values.data(), alignof(OverAligned)), "over-aligned pmr vector data is misaligned");
}

void test_resource_equality()
{
    std::pmr::memory_resource * global = litedb::memory::memory_pool_resource();
    require(global->is_equal(*global), "resource should equal itself");

    litedb::memory::MemoryPoolResource other;
    require(!global->is_equal(other), "different resource instances should not compare equal");
}

} // namespace

int main()
{
    try {
        test_pmr_vector();
        test_over_aligned_vector();
        test_resource_equality();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
