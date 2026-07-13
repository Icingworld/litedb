#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

namespace litedb::tests
{

class TemporaryDirectory
{
public:
    explicit TemporaryDirectory(std::string prefix)
        : path_(std::filesystem::temp_directory_path() /
                (std::move(prefix) + "-" + std::to_string(unique_suffix())))
    {
        std::filesystem::remove_all(path_);
    }

    ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory & operator=(const TemporaryDirectory &) = delete;

    [[nodiscard]]
    const std::filesystem::path & path() const noexcept { return path_; }

private:
    static std::uint64_t unique_suffix() noexcept
    {
        static std::atomic<std::uint64_t> sequence {0};
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        return static_cast<std::uint64_t>(tick) + sequence.fetch_add(1, std::memory_order_relaxed);
    }

    std::filesystem::path path_;
};

} // namespace litedb::tests
