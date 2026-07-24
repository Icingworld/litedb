#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

#include "core/common/ids.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

inline constexpr std::size_t StoragePageSize = 4096;
inline constexpr std::size_t StoragePageHeaderSize = 24;
inline constexpr std::size_t StorageSlotSize = 8;
inline constexpr std::uint16_t StorageFormatVersion = 2;

using StoragePageBuffer = std::array<std::byte, StoragePageSize>;

enum class StorageSlotState : std::uint8_t
{
    Active = 1,
    Deleted = 2,
};

struct StorageFileHeader
{
    common::CollectionId collection_id {0};
    common::RecordId next_record_id {1};
    std::uint32_t page_count {0};
};

struct StorageSlot
{
    std::uint16_t offset {0};
    std::uint16_t length {0};
    StorageSlotState state {StorageSlotState::Deleted};
};

struct StoragePageInfo
{
    std::uint16_t slot_count {0};
    std::uint16_t free_start {StoragePageHeaderSize};
    std::uint16_t free_end {StoragePageSize};
    std::uint32_t generation {0};
    std::vector<StorageSlot> slots;
};

[[nodiscard]]
StoragePageBuffer encode_storage_header(const StorageFileHeader & header);

[[nodiscard]]
std::expected<StorageFileHeader, StorageError> decode_storage_header(
    const StoragePageBuffer & bytes,
    common::CollectionId expected_collection_id
);

[[nodiscard]]
StoragePageBuffer make_storage_page(std::uint32_t page_id);

[[nodiscard]]
std::expected<StoragePageInfo, StorageError> decode_storage_page(
    const StoragePageBuffer & page,
    std::uint32_t expected_page_id
);

void write_storage_page_metadata(
    StoragePageBuffer & page,
    std::uint32_t page_id,
    const StoragePageInfo & info
);

void seal_storage_page(StoragePageBuffer & page);

} // namespace litedb::core::storage
