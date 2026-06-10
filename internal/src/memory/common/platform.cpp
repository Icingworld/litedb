#include "memory/common/platform.hpp"

#if defined(_WIN32)
#    if !defined(NOMINMAX)
#        define NOMINMAX 1
#    endif
#    include <windows.h>
#else
#    include <sys/mman.h>
#endif

namespace litedb::memory
{

void * SystemAllocator::allocate(std::size_t bytes) noexcept
{
    if (bytes == 0) {
        return nullptr;
    }

#if defined(_WIN32)
    return ::VirtualAlloc(
        nullptr,
        bytes,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE
    );
#else
    void * ptr = ::mmap(
        nullptr,
        bytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (ptr == MAP_FAILED) {
        return nullptr;
    }

    return ptr;
#endif
}

void SystemAllocator::deallocate(void * ptr, std::size_t bytes) noexcept
{
    if (ptr == nullptr || bytes == 0) {
        return;
    }

#if defined(_WIN32)
    static_cast<void>(bytes);
    ::VirtualFree(ptr, 0, MEM_RELEASE);
#else
    ::munmap(ptr, bytes);
#endif
}

} // namespace litedb::memory
