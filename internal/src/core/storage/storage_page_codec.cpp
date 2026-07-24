#include "core/storage/storage_page_codec.hpp"

#include <algorithm>
#include <span>

#include "core/io/checksum.hpp"

namespace litedb::core::storage
{

namespace
{

constexpr std::uint32_t StoreMagic = 0x3253444c; // LDS2
constexpr std::uint32_t PageMagic = 0x3247504c;  // LPG2
constexpr std::size_t HeaderChecksumOffset = 36;
constexpr std::size_t PageChecksumOffset = 16;

template <typename T>
T read_number(const std::byte * source)
{
    T value {};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<T>(std::to_integer<unsigned int>(source[index])) << (index * 8U);
    }
    return value;
}

template <typename T>
void write_number(std::byte * target, T value)
{
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        target[index] = static_cast<std::byte>((value >> (index * 8U)) & static_cast<T>(0xffU));
    }
}

std::uint32_t checksum_with_zeroed_field(StoragePageBuffer bytes, std::size_t offset)
{
    write_number(bytes.data() + offset, std::uint32_t {0});
    return io::crc32(bytes);
}

StorageError format_error(StorageErrorCode code, std::string message, std::optional<std::uint32_t> page_id = {})
{
    return make_storage_error(code, std::move(message), {
        .operation = page_id ? StorageOperation::ReadPage : StorageOperation::ReadHeader,
        .page_id = page_id,
    });
}

} // namespace

StoragePageBuffer encode_storage_header(const StorageFileHeader & header)
{
    StoragePageBuffer bytes {};
    write_number(bytes.data(), StoreMagic);
    write_number(bytes.data() + 4, StorageFormatVersion);
    write_number(bytes.data() + 6, static_cast<std::uint16_t>(StoragePageSize));
    write_number(bytes.data() + 8, static_cast<std::uint32_t>(StoragePageSize));
    write_number(bytes.data() + 12, std::uint32_t {0});
    write_number(bytes.data() + 16, static_cast<std::uint64_t>(header.collection_id));
    write_number(bytes.data() + 24, static_cast<std::uint64_t>(header.next_record_id));
    write_number(bytes.data() + 32, header.page_count);
    write_number(bytes.data() + HeaderChecksumOffset, std::uint32_t {0});
    write_number(bytes.data() + HeaderChecksumOffset, io::crc32(bytes));
    return bytes;
}

std::expected<StorageFileHeader, StorageError> decode_storage_header(
    const StoragePageBuffer & bytes,
    common::CollectionId expected_collection_id
)
{
    if (read_number<std::uint32_t>(bytes.data()) != StoreMagic) {
        return std::unexpected(format_error(StorageErrorCode::InvalidFormat, "Invalid storage header magic"));
    }
    if (read_number<std::uint16_t>(bytes.data() + 4) != StorageFormatVersion) {
        return std::unexpected(format_error(StorageErrorCode::UnsupportedVersion, "Unsupported storage format version"));
    }
    if (read_number<std::uint16_t>(bytes.data() + 6) != StoragePageSize ||
        read_number<std::uint32_t>(bytes.data() + 8) != StoragePageSize ||
        read_number<std::uint32_t>(bytes.data() + 12) != 0) {
        return std::unexpected(format_error(StorageErrorCode::InvalidFormat, "Invalid storage header parameters"));
    }
    const auto stored_checksum = read_number<std::uint32_t>(bytes.data() + HeaderChecksumOffset);
    if (stored_checksum != checksum_with_zeroed_field(bytes, HeaderChecksumOffset)) {
        return std::unexpected(format_error(StorageErrorCode::ChecksumMismatch, "Storage header checksum mismatch"));
    }
    if (!std::all_of(bytes.begin() + 40, bytes.end(), [](std::byte value) { return value == std::byte {0}; })) {
        return std::unexpected(format_error(StorageErrorCode::InvalidFormat, "Storage header reserved bytes are non-zero"));
    }

    StorageFileHeader header {
        .collection_id = read_number<std::uint64_t>(bytes.data() + 16),
        .next_record_id = read_number<std::uint64_t>(bytes.data() + 24),
        .page_count = read_number<std::uint32_t>(bytes.data() + 32),
    };
    if (header.collection_id != expected_collection_id || header.next_record_id == 0) {
        return std::unexpected(format_error(StorageErrorCode::InvalidFormat, "Storage header identity or counters are invalid"));
    }
    return header;
}

StoragePageBuffer make_storage_page(std::uint32_t page_id)
{
    StoragePageBuffer page {};
    StoragePageInfo info;
    write_storage_page_metadata(page, page_id, info);
    return page;
}

std::expected<StoragePageInfo, StorageError> decode_storage_page(
    const StoragePageBuffer & page,
    std::uint32_t expected_page_id
)
{
    if (read_number<std::uint32_t>(page.data()) != PageMagic ||
        read_number<std::uint32_t>(page.data() + 4) != expected_page_id) {
        return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Invalid data page header", expected_page_id));
    }
    if (read_number<std::uint16_t>(page.data() + 14) != 0) {
        return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Data page flags are invalid", expected_page_id));
    }
    const auto stored_checksum = read_number<std::uint32_t>(page.data() + PageChecksumOffset);
    if (stored_checksum != checksum_with_zeroed_field(page, PageChecksumOffset)) {
        return std::unexpected(format_error(StorageErrorCode::ChecksumMismatch, "Data page checksum mismatch", expected_page_id));
    }

    StoragePageInfo info {
        .slot_count = read_number<std::uint16_t>(page.data() + 8),
        .free_start = read_number<std::uint16_t>(page.data() + 10),
        .free_end = read_number<std::uint16_t>(page.data() + 12),
        .generation = read_number<std::uint32_t>(page.data() + 20),
    };
    const auto expected_free_start =
        StoragePageHeaderSize + static_cast<std::size_t>(info.slot_count) * StorageSlotSize;
    if (expected_free_start > StoragePageSize || info.free_start != expected_free_start ||
        info.free_start > info.free_end || info.free_end > StoragePageSize) {
        return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Invalid slot directory bounds", expected_page_id));
    }

    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    info.slots.reserve(info.slot_count);
    for (std::uint16_t slot_id = 0; slot_id < info.slot_count; ++slot_id) {
        const auto base = StoragePageHeaderSize + static_cast<std::size_t>(slot_id) * StorageSlotSize;
        const auto state_byte = read_number<std::uint8_t>(page.data() + base + 4);
        if (page[base + 5] != std::byte {0} || page[base + 6] != std::byte {0} ||
            page[base + 7] != std::byte {0}) {
            return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Slot reserved bytes are non-zero", expected_page_id));
        }
        if (state_byte != static_cast<std::uint8_t>(StorageSlotState::Active) &&
            state_byte != static_cast<std::uint8_t>(StorageSlotState::Deleted)) {
            return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Invalid slot state", expected_page_id));
        }
        StorageSlot slot {
            .offset = read_number<std::uint16_t>(page.data() + base),
            .length = read_number<std::uint16_t>(page.data() + base + 2),
            .state = static_cast<StorageSlotState>(state_byte),
        };
        if (slot.length != 0) {
            const auto end = static_cast<std::size_t>(slot.offset) + slot.length;
            if (slot.offset < info.free_end || end > StoragePageSize) {
                return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Slot payload is outside the page", expected_page_id));
            }
            ranges.emplace_back(slot.offset, end);
        } else if (slot.state == StorageSlotState::Active) {
            return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Active slot payload is empty", expected_page_id));
        }
        info.slots.push_back(slot);
    }
    std::sort(ranges.begin(), ranges.end());
    for (std::size_t index = 1; index < ranges.size(); ++index) {
        if (ranges[index].first < ranges[index - 1].second) {
            return std::unexpected(format_error(StorageErrorCode::CorruptedPage, "Slot payloads overlap", expected_page_id));
        }
    }
    return info;
}

void write_storage_page_metadata(
    StoragePageBuffer & page,
    std::uint32_t page_id,
    const StoragePageInfo & info
)
{
    write_number(page.data(), PageMagic);
    write_number(page.data() + 4, page_id);
    write_number(page.data() + 8, info.slot_count);
    write_number(page.data() + 10, info.free_start);
    write_number(page.data() + 12, info.free_end);
    write_number(page.data() + 14, std::uint16_t {0});
    write_number(page.data() + PageChecksumOffset, std::uint32_t {0});
    write_number(page.data() + 20, info.generation);
    seal_storage_page(page);
}

void seal_storage_page(StoragePageBuffer & page)
{
    write_number(page.data() + PageChecksumOffset, std::uint32_t {0});
    write_number(page.data() + PageChecksumOffset, io::crc32(page));
}

} // namespace litedb::core::storage
