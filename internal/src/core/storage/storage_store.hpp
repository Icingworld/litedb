#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <unordered_map>
#include <vector>

#include "core/common/ids.hpp"
#include "core/common/record.hpp"
#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/storage/storage_cursor.hpp"
#include "core/storage/storage_error.hpp"
#include "core/storage/storage_page_codec.hpp"

namespace litedb::core::storage
{

struct PhysicalRid
{
    std::uint32_t page_id;
    std::uint16_t slot_id;
};

struct StorageMetrics
{
    std::uint64_t page_reads {0};
    std::uint64_t page_writes {0};
    std::uint64_t bytes_read {0};
    std::uint64_t bytes_written {0};
    std::uint64_t compactions {0};
    std::uint64_t reused_pages {0};
    std::uint64_t new_pages {0};
    std::uint64_t checksum_failures {0};
};

class StorageStore final
{
private:
    struct PageSpace
    {
        std::size_t contiguous {0};
        std::size_t reclaimable {0};
        bool has_deleted_slot {false};
    };

    StorageStore(
        std::filesystem::path path,
        common::CollectionId collection_id,
        filesystem::FileHandle file
    ) noexcept;

public:
    static constexpr std::uint32_t PageSize = StoragePageSize;

    static std::expected<std::unique_ptr<StorageStore>, StorageError> create(
        std::filesystem::path path,
        common::CollectionId collection_id,
        filesystem::FileSystem & filesystem
    );

    static std::expected<std::unique_ptr<StorageStore>, StorageError> open(
        std::filesystem::path path,
        common::CollectionId collection_id,
        filesystem::FileSystem & filesystem
    );

    [[nodiscard]]
    std::expected<common::Record, StorageError> get(common::RecordId id) const;

    std::expected<common::RecordId, StorageError> insert(common::RecordData data);

    std::expected<void, StorageError> update(common::RecordId id, common::RecordData data);

    std::expected<void, StorageError> erase(common::RecordId id);

    [[nodiscard]]
    std::expected<StorageCursor, StorageError> scan() const;

    [[nodiscard]]
    StorageMetrics metrics() const noexcept;

    [[nodiscard]]
    std::uint32_t page_count() const noexcept;

private:
    std::expected<void, StorageError> initialize();
    std::expected<void, StorageError> load();
    std::expected<void, StorageError> write_header();
    std::expected<StoragePageBuffer, StorageError> load_page(std::uint32_t page_id) const;
    std::expected<void, StorageError> write_page(std::uint32_t page_id, const StoragePageBuffer & page);

    std::expected<PhysicalRid, StorageError> place(
        common::RecordId id,
        const common::RecordData & data,
        std::optional<std::uint32_t> preferred_page = {}
    );

    std::expected<PhysicalRid, StorageError> place_encoded_on_page(
        std::uint32_t page_id,
        StoragePageBuffer & page,
        StoragePageInfo & info,
        std::span<const std::byte> encoded
    );

    std::expected<common::Record, StorageError> read(PhysicalRid rid) const;
    std::expected<void, StorageError> compact_page(
        std::uint32_t page_id,
        StoragePageBuffer & page,
        StoragePageInfo & info
    );

    void update_page_space(std::uint32_t page_id, const StoragePageInfo & info);
    void remove_page_space(std::uint32_t page_id);

private:
    std::filesystem::path path_;
    common::CollectionId collection_id_;
    mutable filesystem::FileHandle file_;
    common::RecordId next_record_id_ {1};
    std::uint32_t page_count_ {0};
    std::map<common::RecordId, PhysicalRid> locations_;
    std::vector<PageSpace> page_spaces_;
    std::set<std::pair<std::size_t, std::uint32_t>> free_space_index_;
    mutable StorageMetrics metrics_;
};

} // namespace litedb::core::storage
