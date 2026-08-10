#pragma once

#ifndef _WIN32

#include <cstdint>
#include <expected>
#include <filesystem>
#include <mutex>
#include <span>

#include "core/filesystem/backend/file_handle_backend.hpp"
#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem::backend
{

class PosixFileHandleBackend final : public FileHandleBackend
{
public:
    PosixFileHandleBackend(int fd, std::filesystem::path path);

    PosixFileHandleBackend(const PosixFileHandleBackend &) = delete;

    PosixFileHandleBackend & operator=(const PosixFileHandleBackend &) = delete;

    ~PosixFileHandleBackend() override;

public:
    std::expected<void, error::Error> close() override;

    std::expected<std::size_t, error::Error>
    read_at(std::uint64_t offset, std::span<std::byte> buffer) override;

    std::expected<void, error::Error>
    write_at(std::uint64_t offset, std::span<const std::byte> data) override;

    std::expected<void, error::Error> append(std::span<const std::byte> data) override;

    std::expected<std::uint64_t, error::Error> size() override;

    std::expected<void, error::Error> truncate(std::uint64_t size) override;

    std::expected<void, error::Error> sync_data() override;

    std::expected<void, error::Error> sync_all() override;

private:
    int fd_ {-1};
    std::filesystem::path path_;
    std::mutex mutex_;
};

} // namespace litedb::core::filesystem::backend

#endif // _WIN32
