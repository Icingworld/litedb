#include "core/storage/storage_store.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/checksum.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

namespace
{

constexpr std::size_t StoragePageSize = 4096;
constexpr std::uint16_t StorageFormatVersion = 1;
constexpr std::uint32_t StorageFileMagic = 0x5342444c; // LDBS
constexpr std::uint32_t StoragePageMagic = 0x5042444c; // LDBP
constexpr std::uint16_t StoragePageHeaderSize = 22;
constexpr std::uint16_t StoragePageSlotSize = 8;
constexpr std::size_t MaxEncodedRecordSize =
    StoragePageSize - StoragePageHeaderSize - StoragePageSlotSize;

enum class StorageSlotState : std::uint8_t
{
    Active = 0,
    Deleted = 1,
};

enum class EncodedValueKind : std::uint8_t
{
    Null = 0,
    Boolean = 1,
    Integer = 2,
    BigInt = 3,
    Float = 4,
    Double = 5,
    String = 6,
    Vector = 7,
};

using StoragePageBuffer = std::array<std::byte, StoragePageSize>;

struct StorageFileHeader
{
    common::CollectionId collection_id {0};
    common::RecordId next_record_id {1};
    std::uint32_t page_count {0};
};

struct StoragePageHeader
{
    std::uint32_t page_id {0};
    std::uint16_t free_start {StoragePageHeaderSize};
    std::uint16_t free_end {static_cast<std::uint16_t>(StoragePageSize)};
    std::uint32_t generation {0};
};

struct StorageSlot
{
    std::uint16_t offset {0};
    std::uint16_t length {0};
    StorageSlotState state {StorageSlotState::Deleted};
};

struct DecodedStoragePage
{
    StoragePageBuffer bytes {};
    StoragePageHeader header {};
    std::vector<StorageSlot> slots;
};

struct PageSpaceData
{
    std::size_t contiguous {0};
    std::size_t reclaimable {0};
    bool has_deleted_slot {false};
};

StorageErrorContext page_context(
    const StorageErrorContext & base,
    std::uint32_t page_id,
    std::optional<std::uint16_t> slot_id = {}
)
{
    auto context = base;
    context.page_id = page_id;
    if (slot_id) {
        context.slot_id = slot_id;
    }
    return context;
}

StorageErrorContext record_context(
    const StorageErrorContext & base,
    common::RecordId record_id,
    std::optional<std::uint32_t> page_id = {},
    std::optional<std::uint16_t> slot_id = {}
)
{
    auto context = base;
    context.record_id = record_id;
    if (page_id) {
        context.page_id = page_id;
    }
    if (slot_id) {
        context.slot_id = slot_id;
    }
    return context;
}

std::expected<void, StorageError>
write_checksum(StoragePageBuffer & bytes, std::size_t checksum_offset)
{
    io::Crc32Calculator calculator;
    calculator.update(std::span<const std::byte> {bytes}.first(checksum_offset));
    calculator.update(
        std::span<const std::byte> {bytes}.subspan(checksum_offset + sizeof(std::uint32_t))
    );

    io::BufferByteWriter checksum_bytes(sizeof(std::uint32_t));
    io::LittleEndianBinaryWriter checksum_writer {checksum_bytes};
    if (auto result = checksum_writer.write_u32(calculator.value()); !result) {
        return std::unexpected(std::move(result.error()));
    }
    std::copy(
        checksum_bytes.bytes().begin(),
        checksum_bytes.bytes().end(),
        bytes.begin() + checksum_offset
    );
    return {};
}

std::expected<StoragePageBuffer, StorageError> encode_header(const StorageFileHeader & header)
{
    io::BufferByteWriter encoded(StoragePageSize);
    io::LittleEndianBinaryWriter writer {encoded};

    if (auto result = writer.write_u32(StorageFileMagic); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(StorageFormatVersion); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(StoragePageSize); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(StoragePageSize); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(0); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(header.collection_id); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(header.next_record_id); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(header.page_count); !result) {
        return std::unexpected(std::move(result.error()));
    }

    StoragePageBuffer bytes {};
    std::copy(encoded.bytes().begin(), encoded.bytes().end(), bytes.begin());
    if (auto checksum = write_checksum(bytes, encoded.bytes().size()); !checksum) {
        return std::unexpected(std::move(checksum.error()));
    }
    return bytes;
}

std::expected<StorageFileHeader, StorageError> decode_header(
    const StoragePageBuffer & bytes,
    common::CollectionId expected_collection_id,
    const StorageErrorContext & context
)
{
    io::BufferByteReader resource {std::span<const std::byte> {bytes}};
    io::LittleEndianBinaryReader reader {
        resource,
        {
            .max_total_bytes = StoragePageSize,
            .max_string_bytes = 0,
        },
    };

    auto magic = reader.read_u32();
    if (!magic) {
        return std::unexpected(std::move(magic.error()));
    }
    if (*magic != StorageFileMagic) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::InvalidFormat, "Invalid storage magic", context)
        );
    }
    auto version = reader.read_u16();
    if (!version) {
        return std::unexpected(std::move(version.error()));
    }
    if (*version != StorageFormatVersion) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::UnsupportedVersion,
            "Unsupported storage format version",
            context
        ));
    }

    auto header_page_size = reader.read_u16();
    if (!header_page_size) {
        return std::unexpected(std::move(header_page_size.error()));
    }
    auto page_size = reader.read_u16();
    if (!page_size) {
        return std::unexpected(std::move(page_size.error()));
    }
    auto flag = reader.read_u16();
    if (!flag) {
        return std::unexpected(std::move(flag.error()));
    }
    if (*header_page_size != StoragePageSize || *page_size != StoragePageSize || *flag != 0) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage header parameters",
            context
        ));
    }

    auto collection_id = reader.read_u64();
    if (!collection_id) {
        return std::unexpected(std::move(collection_id.error()));
    }
    auto next_record_id = reader.read_u64();
    if (!next_record_id) {
        return std::unexpected(std::move(next_record_id.error()));
    }
    auto page_count = reader.read_u32();
    if (!page_count) {
        return std::unexpected(std::move(page_count.error()));
    }

    const auto prefix_size = StoragePageSize - reader.remaining_bytes();
    auto checksum = reader.read_u32();
    if (!checksum) {
        return std::unexpected(std::move(checksum.error()));
    }
    const auto suffix_size = reader.remaining_bytes();
    io::Crc32Calculator calculator;
    calculator.update(std::span<const std::byte> {bytes}.first(prefix_size));
    calculator.update(std::span<const std::byte> {bytes}.last(suffix_size));
    if (*checksum != calculator.value()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ChecksumMismatch,
            "Storage header checksum mismatch",
            context
        ));
    }
    for (const auto byte : std::span<const std::byte> {bytes}.last(suffix_size)) {
        if (byte != std::byte {0}) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidFormat,
                "Invalid storage header reserved bytes",
                context
            ));
        }
    }
    if (*collection_id != expected_collection_id || *next_record_id == 0) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Invalid storage header identity or counters",
            context
        ));
    }

    return StorageFileHeader {
        .collection_id = *collection_id,
        .next_record_id = *next_record_id,
        .page_count = *page_count,
    };
}

std::expected<DecodedStoragePage, StorageError> decode_page(
    StoragePageBuffer bytes,
    std::uint32_t expected_page_id,
    const StorageErrorContext & context
)
{
    io::BufferByteReader resource {std::span<const std::byte> {bytes}};
    io::LittleEndianBinaryReader reader {
        resource,
        {
            .max_total_bytes = StoragePageSize,
            .max_string_bytes = 0,
        },
    };

    auto magic = reader.read_u32();
    if (!magic) {
        return std::unexpected(std::move(magic.error()));
    }
    auto page_id = reader.read_u32();
    if (!page_id) {
        return std::unexpected(std::move(page_id.error()));
    }
    if (*magic != StoragePageMagic || *page_id != expected_page_id) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Invalid storage page header",
            page_context(context, expected_page_id)
        ));
    }

    auto free_start = reader.read_u16();
    if (!free_start) {
        return std::unexpected(std::move(free_start.error()));
    }
    auto free_end = reader.read_u16();
    if (!free_end) {
        return std::unexpected(std::move(free_end.error()));
    }
    auto flag = reader.read_u16();
    if (!flag) {
        return std::unexpected(std::move(flag.error()));
    }
    auto generation = reader.read_u32();
    if (!generation) {
        return std::unexpected(std::move(generation.error()));
    }
    if (*flag != 0 || *free_start < StoragePageHeaderSize || *free_start > StoragePageSize ||
        (*free_start - StoragePageHeaderSize) % StoragePageSlotSize != 0 ||
        *free_end < *free_start || *free_end > StoragePageSize) {
        {
            return std::unexpected(make_storage_error(
                StorageErrorCode::CorruptedPage,
                "Invalid storage page bounds",
                page_context(context, expected_page_id)
            ));
        }
    }

    const auto prefix_size = StoragePageSize - reader.remaining_bytes();
    auto checksum = reader.read_u32();
    if (!checksum) {
        return std::unexpected(std::move(checksum.error()));
    }
    const auto suffix_size = reader.remaining_bytes();
    io::Crc32Calculator calculator;
    calculator.update(std::span<const std::byte> {bytes}.first(prefix_size));
    calculator.update(std::span<const std::byte> {bytes}.last(suffix_size));
    if (*checksum != calculator.value()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ChecksumMismatch,
            "Storage page checksum mismatch",
            page_context(context, expected_page_id)
        ));
    }

    const auto slot_count =
        static_cast<std::uint16_t>((*free_start - StoragePageHeaderSize) / StoragePageSlotSize);
    std::vector<StorageSlot> slots;
    slots.reserve(slot_count);
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    ranges.reserve(slot_count);

    for (std::uint16_t slot_id = 0; slot_id < slot_count; ++slot_id) {
        auto offset = reader.read_u16();
        if (!offset) {
            return std::unexpected(std::move(offset.error()));
        }
        auto length = reader.read_u16();
        if (!length) {
            return std::unexpected(std::move(length.error()));
        }
        auto state = reader.read_u8();
        if (!state) {
            return std::unexpected(std::move(state.error()));
        }
        for (std::size_t index = 0; index < 3; ++index) {
            auto reserved = reader.read_u8();
            if (!reserved) {
                return std::unexpected(std::move(reserved.error()));
            }
            if (*reserved != 0) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::CorruptedPage,
                    "Storage slot reserved bytes are non-zero",
                    page_context(context, expected_page_id, slot_id)
                ));
            }
        }
        if (*state > static_cast<std::uint8_t>(StorageSlotState::Deleted)) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::CorruptedPage,
                "Invalid storage slot state",
                page_context(context, expected_page_id, slot_id)
            ));
        }

        const StorageSlot slot {
            .offset = *offset,
            .length = *length,
            .state = static_cast<StorageSlotState>(*state),
        };
        if (slot.length == 0) {
            if (slot.state != StorageSlotState::Deleted || slot.offset != 0) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::CorruptedPage,
                    "Invalid empty storage slot",
                    page_context(context, expected_page_id, slot_id)
                ));
            }
        } else {
            const auto end = static_cast<std::size_t>(slot.offset) + slot.length;
            if (slot.offset < *free_end || end > StoragePageSize) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::CorruptedPage,
                    "Storage slot payload is outside the page",
                    page_context(context, expected_page_id, slot_id)
                ));
            }
            ranges.emplace_back(slot.offset, end);
        }
        slots.push_back(slot);
    }

    std::ranges::sort(ranges);
    for (std::size_t index = 1; index < ranges.size(); ++index) {
        if (ranges[index].first < ranges[index - 1].second) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::CorruptedPage,
                "Storage slot payloads overlap",
                page_context(context, expected_page_id)
            ));
        }
    }

    return DecodedStoragePage {
        .bytes = std::move(bytes),
        .header =
            {
                .page_id = expected_page_id,
                .free_start = *free_start,
                .free_end = *free_end,
                .generation = *generation,
            },
        .slots = std::move(slots),
    };
}

std::expected<StoragePageBuffer, StorageError>
encode_page(const DecodedStoragePage & page, const StorageErrorContext & context)
{
    if (page.header.free_start < StoragePageHeaderSize ||
        page.header.free_start > StoragePageSize || page.header.free_end < page.header.free_start ||
        page.header.free_end > StoragePageSize ||
        page.slots.size() !=
            (page.header.free_start - StoragePageHeaderSize) / StoragePageSlotSize) {
        {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidState,
                "Invalid in-memory storage page",
                page_context(context, page.header.page_id)
            ));
        }
    }

    io::BufferByteWriter metadata(StoragePageSize);
    io::LittleEndianBinaryWriter writer {metadata};
    if (auto result = writer.write_u32(StoragePageMagic); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(page.header.page_id); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(page.header.free_start); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(page.header.free_end); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(0); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(page.header.generation); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(0); !result) {
        return std::unexpected(std::move(result.error()));
    }
    for (const auto & slot : page.slots) {
        if (auto result = writer.write_u16(slot.offset); !result) {
            return std::unexpected(std::move(result.error()));
        }
        if (auto result = writer.write_u16(slot.length); !result) {
            return std::unexpected(std::move(result.error()));
        }
        if (auto result = writer.write_u8(static_cast<std::uint8_t>(slot.state)); !result) {
            return std::unexpected(std::move(result.error()));
        }
        if (auto result = writer.write_u8(0); !result) {
            return std::unexpected(std::move(result.error()));
        }
        if (auto result = writer.write_u8(0); !result) {
            return std::unexpected(std::move(result.error()));
        }
        if (auto result = writer.write_u8(0); !result) {
            return std::unexpected(std::move(result.error()));
        }
    }

    StoragePageBuffer bytes {};
    std::copy(metadata.bytes().begin(), metadata.bytes().end(), bytes.begin());
    if (page.header.free_end < StoragePageSize) {
        std::copy(
            page.bytes.begin() + page.header.free_end,
            page.bytes.end(),
            bytes.begin() + page.header.free_end
        );
    }
    if (auto checksum = write_checksum(bytes, 18); !checksum) {
        return std::unexpected(std::move(checksum.error()));
    }
    return bytes;
}

std::expected<StoragePageBuffer, StorageError> make_empty_page(std::uint32_t page_id)
{
    const DecodedStoragePage page {
        .bytes {},
        .header {
            .page_id = page_id,
            .free_start = StoragePageHeaderSize,
            .free_end = static_cast<std::uint16_t>(StoragePageSize),
            .generation = 0,
        },
        .slots {},
    };
    return encode_page(page, {});
}

std::expected<StoragePageBuffer, StorageError> read_page_bytes(
    filesystem::FileHandle & file,
    std::uint32_t page_id,
    const StorageErrorContext & context
)
{
    StoragePageBuffer bytes {};
    auto read = file.read_at(static_cast<std::uint64_t>(page_id) * StoragePageSize, bytes);
    if (!read) {
        return std::unexpected(std::move(read.error()));
    }
    if (*read != StoragePageSize) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::UnexpectedEof,
            "Truncated storage page",
            page_context(context, page_id)
        ));
    }
    return bytes;
}

std::expected<DecodedStoragePage, StorageError>
read_page(filesystem::FileHandle & file, std::uint32_t page_id, const StorageErrorContext & context)
{
    auto bytes = read_page_bytes(file, page_id, context);
    if (!bytes) {
        return std::unexpected(std::move(bytes.error()));
    }
    return decode_page(std::move(*bytes), page_id, context);
}

std::expected<void, StorageError> write_page(
    filesystem::FileHandle & file,
    DecodedStoragePage & page,
    const StorageErrorContext & context
)
{
    ++page.header.generation;
    auto bytes = encode_page(page, context);
    if (!bytes) {
        return std::unexpected(std::move(bytes.error()));
    }
    auto written = file.write_at(
        static_cast<std::uint64_t>(page.header.page_id) * StoragePageSize,
        std::span<const std::byte> {*bytes}
    );
    if (!written) {
        return std::unexpected(std::move(written.error()));
    }
    page.bytes = std::move(*bytes);
    return {};
}

std::expected<std::span<const std::byte>, StorageError> active_payload(
    const DecodedStoragePage & page,
    std::uint16_t slot_id,
    const StorageErrorContext & context
)
{
    if (slot_id >= page.slots.size()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidState,
            "Storage slot ID out of range",
            page_context(context, page.header.page_id, slot_id)
        ));
    }
    const auto & slot = page.slots[slot_id];
    if (slot.state != StorageSlotState::Active || slot.length == 0) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidState,
            "Storage slot is not active",
            page_context(context, page.header.page_id, slot_id)
        ));
    }
    return std::span<const std::byte> {page.bytes}.subspan(slot.offset, slot.length);
}

PageSpaceData summarize_page(const DecodedStoragePage & page)
{
    PageSpaceData summary {
        .contiguous = static_cast<std::size_t>(page.header.free_end - page.header.free_start),
        .reclaimable = static_cast<std::size_t>(page.header.free_end - page.header.free_start),
        .has_deleted_slot = false,
    };
    for (const auto & slot : page.slots) {
        if (slot.state == StorageSlotState::Deleted) {
            summary.reclaimable += slot.length;
            summary.has_deleted_slot = true;
        }
    }
    return summary;
}

std::expected<void, StorageError>
compact_page(DecodedStoragePage & page, const StorageErrorContext & context)
{
    StoragePageBuffer compacted {};
    auto free_end = static_cast<std::uint16_t>(StoragePageSize);
    for (auto & slot : page.slots) {
        if (slot.state == StorageSlotState::Active) {
            if (slot.length > free_end) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidState,
                    "Storage page compaction underflow",
                    page_context(context, page.header.page_id)
                ));
            }
            free_end = static_cast<std::uint16_t>(free_end - slot.length);
            std::copy_n(
                page.bytes.begin() + slot.offset,
                slot.length,
                compacted.begin() + free_end
            );
            slot.offset = free_end;
        } else {
            slot.offset = 0;
            slot.length = 0;
        }
    }
    page.bytes = std::move(compacted);
    page.header.free_start =
        static_cast<std::uint16_t>(StoragePageHeaderSize + page.slots.size() * StoragePageSlotSize);
    page.header.free_end = free_end;
    return {};
}

std::expected<std::uint16_t, StorageError> place_encoded(
    DecodedStoragePage & page,
    std::span<const std::byte> encoded,
    const StorageErrorContext & context
)
{
    if (encoded.empty() || encoded.size() > MaxEncodedRecordSize ||
        encoded.size() > std::numeric_limits<std::uint16_t>::max()) {
        {
            return std::unexpected(make_storage_error(
                StorageErrorCode::RecordTooLarge,
                "Encoded record does not fit in a storage page",
                context
            ));
        }
    }
    std::optional<std::uint16_t> reusable_slot;
    for (std::uint16_t slot_id = 0; slot_id < page.slots.size(); ++slot_id) {
        if (page.slots[slot_id].state == StorageSlotState::Deleted) {
            reusable_slot = slot_id;
            break;
        }
    }
    const auto directory_cost = reusable_slot ? 0U : StoragePageSlotSize;
    const auto contiguous = static_cast<std::size_t>(page.header.free_end - page.header.free_start);
    if (contiguous < encoded.size() + directory_cost) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Storage page has insufficient contiguous space",
            page_context(context, page.header.page_id)
        ));
    }

    const auto offset = static_cast<std::uint16_t>(page.header.free_end - encoded.size());
    std::copy(encoded.begin(), encoded.end(), page.bytes.begin() + offset);
    const StorageSlot slot {
        .offset = offset,
        .length = static_cast<std::uint16_t>(encoded.size()),
        .state = StorageSlotState::Active,
    };
    const auto slot_id = reusable_slot.value_or(static_cast<std::uint16_t>(page.slots.size()));
    if (reusable_slot) {
        page.slots[*reusable_slot] = slot;
    } else {
        page.slots.push_back(slot);
        page.header.free_start =
            static_cast<std::uint16_t>(page.header.free_start + StoragePageSlotSize);
    }
    page.header.free_end = offset;
    return slot_id;
}

std::expected<void, StorageError>
erase_slot(DecodedStoragePage & page, std::uint16_t slot_id, const StorageErrorContext & context)
{
    if (slot_id >= page.slots.size() || page.slots[slot_id].state != StorageSlotState::Active) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidState,
            "Storage slot is not active",
            page_context(context, page.header.page_id, slot_id)
        ));
    }
    page.slots[slot_id].state = StorageSlotState::Deleted;
    return {};
}

std::expected<void, StorageError> write_value(
    io::LittleEndianBinaryWriter & writer,
    const common::Value & value,
    const StorageErrorContext & context
)
{
    return std::visit(
        [&writer, &context](const auto & data) -> std::expected<void, StorageError> {
            using T = std::decay_t<decltype(data)>;
            auto write_kind = [&writer](EncodedValueKind kind) {
                return writer.write_u8(static_cast<std::uint8_t>(kind));
            };
            if constexpr (std::is_same_v<T, common::NullValue>) {
                if (auto result = write_kind(EncodedValueKind::Null); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                return {};
            } else if constexpr (std::is_same_v<T, bool>) {
                if (auto result = write_kind(EncodedValueKind::Boolean); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                return writer.write_u8(data ? 1U : 0U);
            } else if constexpr (std::is_same_v<T, std::int32_t>) {
                if (auto result = write_kind(EncodedValueKind::Integer); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                return writer.write_i32(data);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                if (auto result = write_kind(EncodedValueKind::BigInt); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                return writer.write_i64(data);
            } else if constexpr (std::is_same_v<T, float>) {
                if (auto result = write_kind(EncodedValueKind::Float); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                return writer.write_f32(data);
            } else if constexpr (std::is_same_v<T, double>) {
                if (auto result = write_kind(EncodedValueKind::Double); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                return writer.write_f64(data);
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (auto result = write_kind(EncodedValueKind::String); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                return writer.write_string(data);
            } else if constexpr (std::is_same_v<T, common::VectorValue>) {
                if (data.size() > std::numeric_limits<std::uint32_t>::max()) {
                    return std::unexpected(make_storage_error(
                        StorageErrorCode::RecordTooLarge,
                        "Vector is too large to encode",
                        context
                    ));
                }
                if (auto result = write_kind(EncodedValueKind::Vector); !result) {
                    return std::unexpected(std::move(result.error()));
                }
                if (auto result = writer.write_u32(static_cast<std::uint32_t>(data.size()));
                    !result) {
                    return std::unexpected(std::move(result.error()));
                }
                for (const auto element : data) {
                    if (auto result = writer.write_f64(element); !result) {
                        return std::unexpected(std::move(result.error()));
                    }
                }
                return {};
            } else {
                static_assert(!sizeof(T), "invalid value type");
            }
        },
        value.data()
    );
}

std::expected<std::vector<std::byte>, StorageError> encode_record(
    common::RecordId record_id,
    const common::RecordData & data,
    const StorageErrorContext & context
)
{
    io::BufferByteWriter bytes(MaxEncodedRecordSize);
    io::LittleEndianBinaryWriter writer {bytes};
    if (auto result = writer.write_u64(record_id); !result) {
        return std::unexpected(std::move(result.error()));
    }
    if (data.values.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::RecordTooLarge,
            "Record has too many values",
            record_context(context, record_id)
        ));
    }
    if (auto result = writer.write_u32(static_cast<std::uint32_t>(data.values.size())); !result) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::RecordTooLarge,
            "Record header does not fit in a storage page",
            record_context(context, record_id)
        ));
    }
    for (const auto & value : data.values) {
        if (auto result = write_value(writer, value, record_context(context, record_id)); !result) {
            if (result.error().category() == error::ErrorCategory::Io) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::RecordTooLarge,
                    result.error().message(),
                    record_context(context, record_id)
                ));
            }
            return std::unexpected(std::move(result.error()));
        }
    }
    return bytes.take_bytes();
}

std::expected<common::RecordId, StorageError>
decode_record_id(std::span<const std::byte> bytes, const StorageErrorContext & context)
{
    io::BufferByteReader resource {bytes};
    io::LittleEndianBinaryReader reader {
        resource,
        {
            .max_total_bytes = bytes.size(),
            .max_string_bytes = 0,
        },
    };
    auto record_id = reader.read_u64();
    if (!record_id) {
        return std::unexpected(std::move(record_id.error()));
    }
    (void)context;
    return *record_id;
}

std::expected<common::Value, StorageError>
decode_value(io::LittleEndianBinaryReader & reader, const StorageErrorContext & context)
{
    auto kind = reader.read_u8();
    if (!kind) {
        return std::unexpected(std::move(kind.error()));
    }
    if (*kind > static_cast<std::uint8_t>(EncodedValueKind::Vector)) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::InvalidData, "Invalid encoded value kind", context)
        );
    }
    switch (static_cast<EncodedValueKind>(*kind)) {
    case EncodedValueKind::Null:
        return common::Value::null();
    case EncodedValueKind::Boolean: {
        auto value = reader.read_u8();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        if (*value > 1) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::InvalidData,
                "Boolean value must be encoded as zero or one",
                context
            ));
        }
        return common::Value {*value == 1};
    }
    case EncodedValueKind::Integer: {
        auto value = reader.read_i32();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::BigInt: {
        auto value = reader.read_i64();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::Float: {
        auto value = reader.read_f32();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::Double: {
        auto value = reader.read_f64();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {*value};
    }
    case EncodedValueKind::String: {
        auto value = reader.read_string();
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        return common::Value {std::move(*value)};
    }
    case EncodedValueKind::Vector: {
        auto count = reader.read_u32();
        if (!count) {
            return std::unexpected(std::move(count.error()));
        }
        if (*count > reader.remaining_bytes() / sizeof(double)) {
            return std::unexpected(make_storage_error(
                StorageErrorCode::ResourceLimitExceeded,
                "Vector length exceeds remaining binary data",
                context
            ));
        }
        common::VectorValue values;
        values.reserve(*count);
        for (std::uint32_t index = 0; index < *count; ++index) {
            auto value = reader.read_f64();
            if (!value) {
                return std::unexpected(std::move(value.error()));
            }
            values.push_back(*value);
        }
        return common::Value {std::move(values)};
    }
    }
    return std::unexpected(
        make_storage_error(StorageErrorCode::InvalidData, "Invalid encoded value kind", context)
    );
}

std::expected<common::Record, StorageError>
decode_record(std::span<const std::byte> bytes, const StorageErrorContext & context)
{
    if (bytes.size() > MaxEncodedRecordSize) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::RecordTooLarge,
            "Encoded record exceeds the storage page limit",
            context
        ));
    }
    io::BufferByteReader resource {bytes};
    io::LittleEndianBinaryReader reader {
        resource,
        {
            .max_total_bytes = bytes.size(),
            .max_string_bytes = static_cast<std::uint32_t>(
                std::min<std::size_t>(bytes.size(), std::numeric_limits<std::uint32_t>::max())
            ),
        },
    };
    auto record_id = reader.read_u64();
    if (!record_id) {
        return std::unexpected(std::move(record_id.error()));
    }
    if (*record_id == 0) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::InvalidData, "Invalid storage record ID", context)
        );
    }
    auto element_count = reader.read_u32();
    if (!element_count) {
        return std::unexpected(std::move(element_count.error()));
    }
    if (*element_count > reader.remaining_bytes()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Record element count exceeds remaining binary data",
            record_context(context, *record_id)
        ));
    }

    common::Record record;
    record.id = *record_id;
    record.data.values.reserve(*element_count);
    for (std::uint32_t index = 0; index < *element_count; ++index) {
        auto value = decode_value(reader, record_context(context, *record_id));
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        record.data.values.push_back(std::move(*value));
    }
    if (reader.remaining_bytes() != 0) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Storage record contains trailing bytes",
            record_context(context, *record_id)
        ));
    }
    return record;
}

} // namespace

StorageStore::StorageStore(
    std::filesystem::path path,
    common::CollectionId collection_id,
    filesystem::FileHandle file
) noexcept
    : path_(std::move(path))
    , collection_id_(collection_id)
    , file_(std::move(file))
{}

std::expected<std::unique_ptr<StorageStore>, StorageError> StorageStore::create(
    std::filesystem::path path,
    filesystem::FileSystem & filesystem,
    common::CollectionId collection_id
)
{
    if (auto created = filesystem.create_dir_all(path.parent_path()); !created) {
        return std::unexpected(std::move(created.error()));
    }
    auto file = filesystem.open(
        path,
        {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::CreateNew,
        }
    );
    if (!file) {
        return std::unexpected(std::move(file.error()));
    }

    auto store =
        std::unique_ptr<StorageStore>(new StorageStore(path, collection_id, std::move(*file)));
    if (auto result = store->initialize(); !result) {
        (void)store->file_.close();
        (void)filesystem.remove(path);
        return std::unexpected(std::move(result.error()));
    }
    return store;
}

std::expected<std::unique_ptr<StorageStore>, StorageError> StorageStore::open(
    std::filesystem::path path,
    filesystem::FileSystem & filesystem,
    common::CollectionId collection_id
)
{
    auto file = filesystem.open(
        path,
        {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        }
    );
    if (!file) {
        return std::unexpected(std::move(file.error()));
    }
    auto store =
        std::unique_ptr<StorageStore>(new StorageStore(path, collection_id, std::move(*file)));
    if (auto result = store->load(); !result) {
        return std::unexpected(std::move(result.error()));
    }
    return store;
}

std::expected<common::Record, StorageError> StorageStore::get(common::RecordId record_id) const
{
    const auto location = locations_.find(record_id);
    const auto context = StorageErrorContext {
        .operation = StorageOperation::ReadPage,
        .path = path_,
        .collection_id = collection_id_,
        .record_id = record_id,
    };
    if (location == locations_.end()) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::RecordNotFound, "Record not found", context)
        );
    }
    if (location->second.page_id == 0 || location->second.page_id > page_count_) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::InvalidState, "Record page ID is out of range", context)
        );
    }
    auto page = read_page(file_, location->second.page_id, context);
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }
    auto payload = active_payload(*page, location->second.slot_id, context);
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }
    auto record = decode_record(*payload, context);
    if (!record) {
        return std::unexpected(std::move(record.error()));
    }
    if (record->id != record_id) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Storage record ID does not match its location",
            record_context(context, record_id, location->second.page_id, location->second.slot_id)
        ));
    }
    return record;
}

std::expected<common::RecordId, StorageError> StorageStore::insert(common::RecordData data)
{
    const auto context = StorageErrorContext {
        .operation = StorageOperation::Insert,
        .path = path_,
        .collection_id = collection_id_,
    };
    if (next_record_id_ == std::numeric_limits<common::RecordId>::max()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Record ID space is exhausted",
            context
        ));
    }
    const auto record_id = next_record_id_;
    auto encoded = encode_record(record_id, data, context);
    if (!encoded) {
        return std::unexpected(std::move(encoded.error()));
    }
    const auto next_record_id = record_id + 1;

    std::vector<std::uint32_t> candidates;
    for (auto it = free_space_index_.lower_bound({encoded->size(), 0});
         it != free_space_index_.end();
         ++it) {
        const auto page_id = it->second;
        const auto & summary = page_space_summaries_[page_id];
        const auto directory_cost = summary.has_deleted_slot ? 0U : StoragePageSlotSize;
        if (summary.reclaimable >= encoded->size() + directory_cost) {
            candidates.push_back(page_id);
        }
    }
    for (const auto page_id : candidates) {
        auto page = read_page(file_, page_id, context);
        if (!page) {
            return std::unexpected(std::move(page.error()));
        }
        const auto summary = summarize_page(*page);
        const auto directory_cost = summary.has_deleted_slot ? 0U : StoragePageSlotSize;
        if (summary.contiguous < encoded->size() + directory_cost) {
            if (auto compacted = compact_page(*page, context); !compacted) {
                return std::unexpected(std::move(compacted.error()));
            }
        }
        auto slot_id = place_encoded(*page, *encoded, record_context(context, record_id, page_id));
        if (!slot_id) {
            continue;
        }
        if (auto written = write_page(file_, *page, context); !written) {
            return std::unexpected(std::move(written.error()));
        }
        auto header = encode_header({collection_id_, next_record_id, page_count_});
        if (!header) {
            return std::unexpected(std::move(header.error()));
        }
        if (auto written_header = file_.write_at(0, std::span<const std::byte> {*header});
            !written_header)
            return std::unexpected(std::move(written_header.error()));
        locations_.emplace(record_id, PhysicalRid {page_id, *slot_id});
        next_record_id_ = next_record_id;
        const auto summary_after = summarize_page(*page);
        update_page_space(
            page_id,
            PageSpaceSummary {
                summary_after.contiguous,
                summary_after.reclaimable,
                summary_after.has_deleted_slot
            }
        );
        return record_id;
    }

    if (page_count_ == std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Storage page ID space is exhausted",
            context
        ));
    }
    const auto page_id = page_count_ + 1;
    auto empty = make_empty_page(page_id);
    if (!empty) {
        return std::unexpected(std::move(empty.error()));
    }
    auto page = decode_page(std::move(*empty), page_id, context);
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }
    auto slot_id = place_encoded(*page, *encoded, record_context(context, record_id, page_id));
    if (!slot_id) {
        return std::unexpected(std::move(slot_id.error()));
    }
    if (auto written = write_page(file_, *page, context); !written) {
        return std::unexpected(std::move(written.error()));
    }
    auto header = encode_header({collection_id_, next_record_id, page_id});
    if (!header) {
        return std::unexpected(std::move(header.error()));
    }
    if (auto written_header = file_.write_at(0, std::span<const std::byte> {*header});
        !written_header)
        return std::unexpected(std::move(written_header.error()));
    page_count_ = page_id;
    page_space_summaries_.resize(static_cast<std::size_t>(page_count_) + 1);
    locations_.emplace(record_id, PhysicalRid {page_id, *slot_id});
    next_record_id_ = next_record_id;
    const auto summary_after = summarize_page(*page);
    update_page_space(
        page_id,
        PageSpaceSummary {
            summary_after.contiguous,
            summary_after.reclaimable,
            summary_after.has_deleted_slot
        }
    );
    return record_id;
}

std::expected<void, StorageError>
StorageStore::update(common::RecordId record_id, common::RecordData data)
{
    const auto context = StorageErrorContext {
        .operation = StorageOperation::Update,
        .path = path_,
        .collection_id = collection_id_,
        .record_id = record_id,
    };
    const auto location = locations_.find(record_id);
    if (location == locations_.end()) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::RecordNotFound, "Record not found", context)
        );
    }
    auto encoded = encode_record(record_id, data, context);
    if (!encoded) {
        return std::unexpected(std::move(encoded.error()));
    }

    const auto old_location = location->second;
    auto source = read_page(file_, old_location.page_id, context);
    if (!source) {
        return std::unexpected(std::move(source.error()));
    }
    auto old_payload = active_payload(*source, old_location.slot_id, context);
    if (!old_payload) {
        return std::unexpected(std::move(old_payload.error()));
    }
    auto old_id = decode_record_id(*old_payload, context);
    if (!old_id) {
        return std::unexpected(std::move(old_id.error()));
    }
    if (*old_id != record_id) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Stored record ID does not match its location",
            context
        ));
    }

    if (auto erased = erase_slot(*source, old_location.slot_id, context); !erased) {
        return std::unexpected(std::move(erased.error()));
    }
    if (auto compacted = compact_page(*source, context); !compacted) {
        return std::unexpected(std::move(compacted.error()));
    }
    if (auto replacement = place_encoded(*source, *encoded, context); replacement) {
        if (auto written = write_page(file_, *source, context); !written) {
            return std::unexpected(std::move(written.error()));
        }
        location->second = PhysicalRid {old_location.page_id, *replacement};
        const auto summary_after = summarize_page(*source);
        update_page_space(
            old_location.page_id,
            PageSpaceSummary {
                summary_after.contiguous,
                summary_after.reclaimable,
                summary_after.has_deleted_slot
            }
        );
        return {};
    }

    std::vector<std::uint32_t> candidates;
    for (auto it = free_space_index_.lower_bound({encoded->size(), 0});
         it != free_space_index_.end();
         ++it) {
        if (it->second != old_location.page_id) {
            candidates.push_back(it->second);
        }
    }
    for (const auto page_id : candidates) {
        auto destination = read_page(file_, page_id, context);
        if (!destination) {
            return std::unexpected(std::move(destination.error()));
        }
        const auto summary = summarize_page(*destination);
        const auto directory_cost = summary.has_deleted_slot ? 0U : StoragePageSlotSize;
        if (summary.contiguous < encoded->size() + directory_cost) {
            if (auto compacted = compact_page(*destination, context); !compacted) {
                return std::unexpected(std::move(compacted.error()));
            }
        }
        auto replacement = place_encoded(*destination, *encoded, context);
        if (!replacement) {
            continue;
        }
        if (auto written = write_page(file_, *destination, context); !written) {
            return std::unexpected(std::move(written.error()));
        }
        if (auto written_source = write_page(file_, *source, context); !written_source) {
            return std::unexpected(std::move(written_source.error()));
        }
        location->second = PhysicalRid {page_id, *replacement};
        const auto source_summary = summarize_page(*source);
        const auto destination_summary = summarize_page(*destination);
        update_page_space(
            old_location.page_id,
            PageSpaceSummary {
                source_summary.contiguous,
                source_summary.reclaimable,
                source_summary.has_deleted_slot
            }
        );
        update_page_space(
            page_id,
            PageSpaceSummary {
                destination_summary.contiguous,
                destination_summary.reclaimable,
                destination_summary.has_deleted_slot
            }
        );
        return {};
    }

    if (page_count_ == std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::ResourceLimitExceeded,
            "Storage page ID space is exhausted",
            context
        ));
    }
    const auto page_id = page_count_ + 1;
    auto empty = make_empty_page(page_id);
    if (!empty) {
        return std::unexpected(std::move(empty.error()));
    }
    auto destination = decode_page(std::move(*empty), page_id, context);
    if (!destination) {
        return std::unexpected(std::move(destination.error()));
    }
    auto replacement = place_encoded(*destination, *encoded, context);
    if (!replacement) {
        return std::unexpected(std::move(replacement.error()));
    }
    if (auto written = write_page(file_, *destination, context); !written) {
        return std::unexpected(std::move(written.error()));
    }
    if (auto written_source = write_page(file_, *source, context); !written_source) {
        return std::unexpected(std::move(written_source.error()));
    }
    auto header = encode_header({collection_id_, next_record_id_, page_id});
    if (!header) {
        return std::unexpected(std::move(header.error()));
    }
    if (auto written_header = file_.write_at(0, std::span<const std::byte> {*header});
        !written_header)
        return std::unexpected(std::move(written_header.error()));
    page_count_ = page_id;
    page_space_summaries_.resize(static_cast<std::size_t>(page_count_) + 1);
    location->second = PhysicalRid {page_id, *replacement};
    const auto source_summary = summarize_page(*source);
    const auto destination_summary = summarize_page(*destination);
    update_page_space(
        old_location.page_id,
        PageSpaceSummary {
            source_summary.contiguous,
            source_summary.reclaimable,
            source_summary.has_deleted_slot
        }
    );
    update_page_space(
        page_id,
        PageSpaceSummary {
            destination_summary.contiguous,
            destination_summary.reclaimable,
            destination_summary.has_deleted_slot
        }
    );
    return {};
}

std::expected<void, StorageError> StorageStore::erase(common::RecordId record_id)
{
    const auto context = StorageErrorContext {
        .operation = StorageOperation::Erase,
        .path = path_,
        .collection_id = collection_id_,
        .record_id = record_id,
    };
    const auto location = locations_.find(record_id);
    if (location == locations_.end()) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::RecordNotFound, "Record not found", context)
        );
    }
    auto page = read_page(file_, location->second.page_id, context);
    if (!page) {
        return std::unexpected(std::move(page.error()));
    }
    auto payload = active_payload(*page, location->second.slot_id, context);
    if (!payload) {
        return std::unexpected(std::move(payload.error()));
    }
    auto stored_id = decode_record_id(*payload, context);
    if (!stored_id) {
        return std::unexpected(std::move(stored_id.error()));
    }
    if (*stored_id != record_id) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::CorruptedPage,
            "Stored record ID does not match its location",
            context
        ));
    }
    if (auto erased = erase_slot(*page, location->second.slot_id, context); !erased) {
        return std::unexpected(std::move(erased.error()));
    }
    if (auto written = write_page(file_, *page, context); !written) {
        return std::unexpected(std::move(written.error()));
    }
    locations_.erase(location);
    const auto summary_after = summarize_page(*page);
    update_page_space(
        page->header.page_id,
        PageSpaceSummary {
            summary_after.contiguous,
            summary_after.reclaimable,
            summary_after.has_deleted_slot
        }
    );
    return {};
}

std::expected<StorageCursor, StorageError> StorageStore::scan() const
{
    const auto context = StorageErrorContext {
        .operation = StorageOperation::Scan,
        .path = path_,
        .collection_id = collection_id_,
    };
    std::vector<common::Record> records;
    records.reserve(locations_.size());
    for (std::uint32_t page_id = 1; page_id <= page_count_; ++page_id) {
        auto page = read_page(file_, page_id, context);
        if (!page) {
            return std::unexpected(std::move(page.error()));
        }
        for (std::uint16_t slot_id = 0; slot_id < page->slots.size(); ++slot_id) {
            if (page->slots[slot_id].state != StorageSlotState::Active) {
                continue;
            }
            auto payload = active_payload(*page, slot_id, context);
            if (!payload) {
                return std::unexpected(std::move(payload.error()));
            }
            auto record = decode_record(*payload, page_context(context, page_id, slot_id));
            if (!record) {
                return std::unexpected(std::move(record.error()));
            }
            const auto location = locations_.find(record->id);
            if (location == locations_.end() || location->second.page_id != page_id ||
                location->second.slot_id != slot_id) {
                {
                    return std::unexpected(make_storage_error(
                        StorageErrorCode::CorruptedPage,
                        "Storage location index does not match page contents",
                        record_context(context, record->id, page_id, slot_id)
                    ));
                }
            }
            records.push_back(std::move(*record));
        }
    }
    if (records.size() != locations_.size()) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidState,
            "Storage location index contains records absent from scanned pages",
            context
        ));
    }
    return StorageCursor {std::move(records)};
}

std::expected<void, StorageError> StorageStore::initialize()
{
    next_record_id_ = 1;
    page_count_ = 0;
    locations_.clear();
    page_space_summaries_.clear();
    free_space_index_.clear();
    return write_header();
}

std::expected<void, StorageError> StorageStore::write_header()
{
    const auto context = StorageErrorContext {
        .operation = StorageOperation::WriteHeader,
        .path = path_,
        .collection_id = collection_id_,
    };
    auto header = encode_header({collection_id_, next_record_id_, page_count_});
    if (!header) {
        return std::unexpected(std::move(header.error()));
    }
    auto written = file_.write_at(0, std::span<const std::byte> {*header});
    if (!written) {
        return std::unexpected(std::move(written.error()));
    }
    return {};
}

std::expected<void, StorageError> StorageStore::load()
{
    const auto context = StorageErrorContext {
        .operation = StorageOperation::Load,
        .path = path_,
        .collection_id = collection_id_,
    };
    auto size = file_.size();
    if (!size) {
        return std::unexpected(std::move(size.error()));
    }
    if (*size < StoragePageSize) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::UnexpectedEof, "Truncated storage header", context)
        );
    }
    if (*size % StoragePageSize != 0) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::InvalidFormat, "Invalid storage file size", context)
        );
    }

    StoragePageBuffer header_bytes {};
    auto header_read = file_.read_at(0, header_bytes);
    if (!header_read) {
        return std::unexpected(std::move(header_read.error()));
    }
    if (*header_read != StoragePageSize) {
        return std::unexpected(
            make_storage_error(StorageErrorCode::UnexpectedEof, "Truncated storage header", context)
        );
    }
    auto header = decode_header(header_bytes, collection_id_, context);
    if (!header) {
        return std::unexpected(std::move(header.error()));
    }
    const auto expected_page_count = (*size - StoragePageSize) / StoragePageSize;
    if (header->page_count != expected_page_count) {
        return std::unexpected(make_storage_error(
            StorageErrorCode::InvalidFormat,
            "Storage page count does not match file size",
            context
        ));
    }

    std::map<common::RecordId, PhysicalRid> locations;
    std::vector<PageSpaceSummary> page_summaries(static_cast<std::size_t>(header->page_count) + 1);
    std::set<std::pair<std::size_t, std::uint32_t>> free_space_index;
    for (std::uint32_t page_id = 1; page_id <= header->page_count; ++page_id) {
        auto page = read_page(file_, page_id, context);
        if (!page) {
            return std::unexpected(std::move(page.error()));
        }
        const auto summary = summarize_page(*page);
        page_summaries[page_id] =
            PageSpaceSummary {summary.contiguous, summary.reclaimable, summary.has_deleted_slot};
        free_space_index.emplace(summary.reclaimable, page_id);
        for (std::uint16_t slot_id = 0; slot_id < page->slots.size(); ++slot_id) {
            if (page->slots[slot_id].state != StorageSlotState::Active) {
                continue;
            }
            auto payload = active_payload(*page, slot_id, context);
            if (!payload) {
                return std::unexpected(std::move(payload.error()));
            }
            auto record_id = decode_record_id(*payload, page_context(context, page_id, slot_id));
            if (!record_id) {
                return std::unexpected(std::move(record_id.error()));
            }
            if (*record_id == 0 || *record_id >= header->next_record_id) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidFormat,
                    "Invalid storage record ID",
                    record_context(context, *record_id, page_id, slot_id)
                ));
            }
            if (!locations.emplace(*record_id, PhysicalRid {page_id, slot_id}).second) {
                return std::unexpected(make_storage_error(
                    StorageErrorCode::InvalidFormat,
                    "Duplicate storage record ID",
                    record_context(context, *record_id, page_id, slot_id)
                ));
            }
        }
    }

    next_record_id_ = header->next_record_id;
    page_count_ = header->page_count;
    locations_ = std::move(locations);
    page_space_summaries_ = std::move(page_summaries);
    free_space_index_ = std::move(free_space_index);
    return {};
}

void StorageStore::update_page_space(std::uint32_t page_id, const PageSpaceSummary & summary)
{
    if (page_id >= page_space_summaries_.size()) {
        page_space_summaries_.resize(static_cast<std::size_t>(page_id) + 1);
    }
    free_space_index_.erase({page_space_summaries_[page_id].reclaimable, page_id});
    page_space_summaries_[page_id] = summary;
    free_space_index_.emplace(summary.reclaimable, page_id);
}

} // namespace litedb::core::storage
